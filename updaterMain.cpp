#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVersionNumber>
#include <QVBoxLayout>
#include <QVariant>

namespace
{
constexpr const char *kCurrentVersion = TIMETABLE_VERSION;
constexpr const char *kLatestReleaseApiUrl =
    "https://api.github.com/repos/ROMEKANA/TimeTable/releases/latest";
constexpr const char *kReleasesPageUrl =
    "https://github.com/ROMEKANA/TimeTable/releases";

// リリースタグから比較可能なバージョン番号を取得する
QVersionNumber versionFromTag(QString tag)
{
    tag = tag.trimmed();

    if (tag.startsWith('v', Qt::CaseInsensitive))
    {
        tag.remove(0, 1);
    }

    const int suffixIndex = tag.indexOf('-');

    if (suffixIndex >= 0)
    {
        tag = tag.left(suffixIndex);
    }

    return QVersionNumber::fromString(tag).normalized();
}

// ISO形式の公開日時をローカル日時の表示文字列へ変換する
QString releaseDateText(const QString &publishedAt)
{
    const QDateTime dateTime = QDateTime::fromString(publishedAt, Qt::ISODate);

    if (!dateTime.isValid())
    {
        return QString();
    }

    return dateTime.toLocalTime().toString("yyyy/MM/dd HH:mm");
}

// 文字列をPowerShellの単一引用符リテラルとして安全に囲む
QString quotedPowerShellString(QString text)
{
    text.replace('\'', "''");
    return QString("'%1'").arg(text);
}

class UpdaterDialog : public QDialog
{
public:
    // 更新画面を構築して最新版の確認を開始する
    explicit UpdaterDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("TimeTable Updater");
        resize(500, 260);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        auto *titleLabel = new QLabel("TimeTable 更新確認", this);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 3);
        titleLabel->setFont(titleFont);

        currentVersionLabel = new QLabel(
            QString("現在のバージョン: %1").arg(kCurrentVersion),
            this);
        statusLabel = new QLabel(this);
        statusLabel->setWordWrap(true);
        detailLabel = new QLabel(this);
        detailLabel->setWordWrap(true);
        detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        progressBar = new QProgressBar(this);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        progressBar->setVisible(false);

        auto *buttonLayout = new QHBoxLayout();
        checkButton = new QPushButton("再確認", this);
        updateButton = new QPushButton("更新する", this);
        releasePageButton = new QPushButton("リリースページを開く", this);
        auto *closeButton = new QPushButton("閉じる", this);

        updateButton->setEnabled(false);
        releasePageButton->setEnabled(false);

        buttonLayout->addWidget(checkButton);
        buttonLayout->addWidget(updateButton);
        buttonLayout->addWidget(releasePageButton);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);

        layout->addWidget(titleLabel);
        layout->addWidget(currentVersionLabel);
        layout->addWidget(statusLabel);
        layout->addWidget(detailLabel, 1);
        layout->addWidget(progressBar);
        layout->addLayout(buttonLayout);

        connect(
            checkButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                checkLatestRelease();
            });
        connect(
            updateButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                downloadAndInstallLatestRelease();
            });
        connect(
            releasePageButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (validReleaseUrl(latestReleasePageUrl))
                {
                    QDesktopServices::openUrl(latestReleasePageUrl);
                }
            });
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

        QTimer::singleShot(
            0,
            this,
            [this]()
            {
                checkLatestRelease();
            });
    }

private:
    // GitHub Releasesへ最新版を問い合わせる
    void checkLatestRelease()
    {
        checkButton->setEnabled(false);
        updateButton->setEnabled(false);
        releasePageButton->setEnabled(false);
        latestReleasePageUrl = QUrl();
        latestDownloadUrl = QUrl();
        latestAssetName.clear();
        latestTagName.clear();
        progressBar->setVisible(false);
        progressBar->setValue(0);
        statusLabel->setText("GitHub Releases の最新版を確認しています...");
        detailLabel->clear();

        QNetworkRequest request{QUrl(kLatestReleaseApiUrl)};
        request.setHeader(QNetworkRequest::UserAgentHeader, "TimeTableUpdater");
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

        QNetworkReply *reply = network.get(request);

        connect(
            reply,
            &QNetworkReply::finished,
            this,
            [this, reply]()
            {
                handleReleaseReply(reply);
                reply->deleteLater();
                checkButton->setEnabled(true);
            });
    }

    // 更新リンクをこのリポジトリのHTTPS URLに限定する。
    bool validReleaseUrl(const QUrl &url) const
    {
        return !url.isEmpty() && url.isValid() && url.scheme() == "https" &&
            url.host() == "github.com" && url.path().startsWith("/ROMEKANA/TimeTable/releases/");
    }

    // 最新リリースの応答を解析して更新可否を表示する
    void handleReleaseReply(QNetworkReply *reply)
    {
        if (reply->error() != QNetworkReply::NoError)
        {
            const int statusCode =
                reply
                    ->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                    .toInt();

            if (statusCode == 404)
            {
                latestReleasePageUrl = QUrl(kReleasesPageUrl);
                releasePageButton->setEnabled(true);
                statusLabel->setText("公開済みリリースがまだ見つかりません。");
                detailLabel->setText(
                    "GitHub Releases に v0.1.0 などのリリースを作成すると、"
                    "ここで最新版を確認できるようになります。");
                return;
            }

            statusLabel->setText("更新確認に失敗しました。");
            detailLabel->setText(reply->errorString());
            return;
        }

        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());

        if (!document.isObject())
        {
            statusLabel->setText("更新情報を読み取れませんでした。");
            detailLabel->setText("GitHub Releases の応答形式が想定と異なります。");
            return;
        }

        const QJsonObject release = document.object();
        const QString tagName = release.value("tag_name").toString().trimmed();
        const QString releaseName = release.value("name").toString().trimmed();
        const QString htmlUrl = release.value("html_url").toString().trimmed();
        const QString publishedAt = release.value("published_at").toString().trimmed();
        const QJsonArray assets = release.value("assets").toArray();

        latestTagName = tagName;
        latestReleasePageUrl = QUrl(htmlUrl);
        releasePageButton->setEnabled(validReleaseUrl(latestReleasePageUrl));

        const QVersionNumber currentVersion = versionFromTag(kCurrentVersion);
        const QVersionNumber latestVersion = versionFromTag(tagName);
        const int comparison = QVersionNumber::compare(latestVersion, currentVersion);

        QJsonObject selectedAsset;

        for (const QJsonValue &assetValue : assets)
        {
            const QJsonObject asset = assetValue.toObject();
            const QString assetName = asset.value("name").toString().trimmed();
            const QString downloadUrl =
                asset.value("browser_download_url").toString().trimmed();

            if (downloadUrl.isEmpty() || !assetName.endsWith(".zip", Qt::CaseInsensitive))
            {
                continue;
            }

            selectedAsset = asset;

            if (assetName.contains("win64", Qt::CaseInsensitive))
            {
                break;
            }
        }

        if (!selectedAsset.isEmpty())
        {
            latestAssetName = QFileInfo(selectedAsset.value("name").toString().trimmed()).fileName();
            latestAssetSize = selectedAsset.value("size").toInteger();
            latestAssetDigest = selectedAsset.value("digest").toString();
            latestDownloadUrl =
                QUrl(selectedAsset.value("browser_download_url").toString().trimmed());
        }

        if (tagName.isEmpty() || latestVersion.isNull())
        {
            statusLabel->setText("最新リリースのバージョンを判定できませんでした。");
        }
        else if (comparison > 0)
        {
            statusLabel->setText(
                QString("新しいバージョンがあります: %1 -> %2")
                    .arg(kCurrentVersion, tagName));
            updateButton->setEnabled(validReleaseUrl(latestDownloadUrl));
        }
        else
        {
            statusLabel->setText(
                QString("最新版です: %1").arg(kCurrentVersion));
        }

        QString assetText = "配布ファイル: なし";

        if (!selectedAsset.isEmpty())
        {
            if (!latestAssetName.isEmpty())
            {
                assetText = QString("配布ファイル: %1").arg(latestAssetName);
            }

            if (validReleaseUrl(latestDownloadUrl))
            {
                assetText += QString("\nダウンロードURL: %1").arg(latestDownloadUrl.toString());
            }
        }

        detailLabel->setText(
            QString("リリース: %1\nタグ: %2\n公開日時: %3\n%4")
                .arg(
                    releaseName.isEmpty() ? tagName : releaseName,
                    tagName,
                    releaseDateText(publishedAt),
                    assetText));
    }

    // 最新版の配布ZIPをダウンロードする
    void downloadAndInstallLatestRelease()
    {
        if (!validReleaseUrl(latestDownloadUrl))
        {
            QMessageBox::warning(
                this,
                "更新できません",
                "ダウンロードできる配布zipが見つかりません。");
            return;
        }

        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            "更新の確認",
            QString("TimeTable を %1 に更新します。\n"
                    "更新中は TimeTable を自動で閉じ、ファイルを置き換えます。")
                .arg(latestTagName.isEmpty() ? "最新版" : latestTagName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (answer != QMessageBox::Yes)
        {
            return;
        }

        checkButton->setEnabled(false);
        updateButton->setEnabled(false);
        releasePageButton->setEnabled(false);
        progressBar->setVisible(true);
        progressBar->setValue(0);
        statusLabel->setText("更新ファイルをダウンロードしています...");

        QNetworkRequest request{latestDownloadUrl};
        request.setHeader(QNetworkRequest::UserAgentHeader, "TimeTableUpdater");
        QNetworkReply *reply = network.get(request);

        connect(
            reply,
            &QNetworkReply::downloadProgress,
            this,
            [this](qint64 bytesReceived, qint64 bytesTotal)
            {
                if (bytesTotal <= 0)
                {
                    progressBar->setRange(0, 0);
                    return;
                }

                progressBar->setRange(0, 100);
                progressBar->setValue(static_cast<int>(bytesReceived * 100 / bytesTotal));
            });
        connect(
            reply,
            &QNetworkReply::finished,
            this,
            [this, reply]()
            {
                handleDownloadReply(reply);
                reply->deleteLater();
            });
    }

    // ダウンロードしたZIPを保存して更新処理を開始する
    void handleDownloadReply(QNetworkReply *reply)
    {
        if (reply->error() != QNetworkReply::NoError)
        {
            statusLabel->setText("更新ファイルのダウンロードに失敗しました。");
            detailLabel->setText(reply->errorString());
            checkButton->setEnabled(true);
            updateButton->setEnabled(validReleaseUrl(latestDownloadUrl));
            releasePageButton->setEnabled(validReleaseUrl(latestReleasePageUrl));
            progressBar->setVisible(false);
            return;
        }

        const QString tempRoot =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString updateDir =
            QDir(tempRoot).filePath("TimeTableUpdate-" + QUuid::createUuid().toString(QUuid::Id128));

        if (!QDir().mkpath(updateDir))
        {
            checkButton->setEnabled(true);
            updateButton->setEnabled(validReleaseUrl(latestDownloadUrl));
            releasePageButton->setEnabled(validReleaseUrl(latestReleasePageUrl));
            progressBar->setVisible(false);
            statusLabel->setText("更新用の一時フォルダを作成できませんでした。");
            return;
        }

        const QString assetName =
            latestAssetName.isEmpty() ? QString("TimeTable-update.zip") : latestAssetName;
        const QString zipPath = QDir(updateDir).filePath(assetName);
        QSaveFile zipFile(zipPath);
        zipFile.setDirectWriteFallback(false);

        if (!zipFile.open(QIODevice::WriteOnly))
        {
            statusLabel->setText("更新ファイルを保存できませんでした。");
            detailLabel->setText(zipFile.errorString());
            checkButton->setEnabled(true);
            updateButton->setEnabled(validReleaseUrl(latestDownloadUrl));
            releasePageButton->setEnabled(validReleaseUrl(latestReleasePageUrl));
            progressBar->setVisible(false);
            return;
        }

        const QByteArray archive = reply->readAll();
        const QString hash = "sha256:" + QString::fromLatin1(QCryptographicHash::hash(archive, QCryptographicHash::Sha256).toHex());
        if (archive.size() != latestAssetSize || !archive.startsWith("PK") ||
            (!latestAssetDigest.isEmpty() && latestAssetDigest != hash) ||
            zipFile.write(archive) != archive.size() || !zipFile.commit())
        {
            statusLabel->setText("更新ファイルの検証または保存に失敗しました。インストール先は変更していません。");
            checkButton->setEnabled(true);
            updateButton->setEnabled(validReleaseUrl(latestDownloadUrl));
            releasePageButton->setEnabled(validReleaseUrl(latestReleasePageUrl));
            progressBar->setVisible(false);
            return;
        }

        if (!startInstallHelper(zipPath))
        {
            checkButton->setEnabled(true);
            updateButton->setEnabled(validReleaseUrl(latestDownloadUrl));
            releasePageButton->setEnabled(validReleaseUrl(latestReleasePageUrl));
            progressBar->setVisible(false);
            return;
        }

        statusLabel->setText("更新を開始します。Updater を終了してファイルを置き換えます...");
        detailLabel->setText("置き換え完了後、TimeTable を自動で起動します。");
        QTimer::singleShot(800, qApp, &QCoreApplication::quit);
    }

    // アプリ終了後にZIPを展開する更新用スクリプトを起動する
    bool startInstallHelper(const QString &zipPath)
    {
        const QString installDir = QCoreApplication::applicationDirPath();
        const QString helperPath =
            QFileInfo(zipPath).absoluteDir().filePath("install-update.ps1");
        QSaveFile helperFile(helperPath);
        helperFile.setDirectWriteFallback(false);

        if (!helperFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            statusLabel->setText("更新用スクリプトを作成できませんでした。");
            detailLabel->setText(helperFile.errorString());
            return false;
        }

        const QString script = QString(R"PS($ErrorActionPreference = 'Stop'
$zip = %1
$install = %2
$updaterPid = %3
$work = Split-Path -Parent $zip
$extract = Join-Path $work 'extract'
$log = Join-Path $work 'update-error.log'
$updateLock = $null
$updateLockPath = Join-Path $install '.timetable-update.lock'
Set-Location -LiteralPath $env:TEMP

function Get-TimeTableProcess {
  Get-Process -Name TimeTable, TimeTableUpdater -ErrorAction SilentlyContinue | Where-Object {
    try {
      $_.Path -and ([IO.Path]::GetDirectoryName($_.Path) -eq $install)
    } catch {
      $false
    }
  }
}

function Wait-TimeTableProcesses {
  param([int]$Seconds)
  $limit = (Get-Date).AddSeconds($Seconds)

  while ((Get-Date) -lt $limit) {
    $running = @(Get-TimeTableProcess)

    if ($running.Count -eq 0) {
      return
    }

    Start-Sleep -Milliseconds 500
  }

  $running = @(Get-TimeTableProcess)

  if ($running.Count -gt 0) {
    throw "TimeTable is still running. Update cancelled; save your work and close the application before retrying."
  }

  Start-Sleep -Seconds 2
}

function Wait-UpdaterExit {
  $limit = (Get-Date).AddSeconds(30)

  while ((Get-Date) -lt $limit) {
    $updater = Get-Process -Id $updaterPid -ErrorAction SilentlyContinue

    if ($null -eq $updater) {
      return
    }

    Start-Sleep -Milliseconds 500
  }

  throw "Updater is still running. Update cancelled."
}

function Get-UpdateFileHash {
  param([string]$Path)
  $stream = [IO.File]::OpenRead($Path)
  $algorithm = [Security.Cryptography.SHA256]::Create()
  try { return [BitConverter]::ToString($algorithm.ComputeHash($stream)) }
  finally { $algorithm.Dispose(); $stream.Dispose() }
}

function Install-VerifiedFiles {
  $required = @('TimeTable.exe', 'TimeTableUpdater.exe', 'Qt6Core.dll', 'Qt6Widgets.dll', 'platforms/qwindows.dll')
  foreach ($name in $required) {
    $path = Join-Path $extract $name
    if (!(Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) {
      throw "Required update file missing: $name"
    }
  }
  $backup = Join-Path $work 'original-app'
  New-Item -ItemType Directory -Path $backup | Out-Null
  $files = @(Get-ChildItem -LiteralPath $extract -File -Recurse)
  $prepared = @()
  foreach ($file in $files) {
    $relative = $file.FullName.Substring($extract.Length + 1)
    # 保存データとユーザーの出力は更新ZIPに含まれていても取り込まない。
    $topDirectory = $relative.Replace('\', '/').Split('/')[0]
    if ($topDirectory -in @('data', 'schedules', 'backups', 'schedulePDF', 'guidanceReportPDF', 'salaryPDF') -or $relative -match '\.lock$') { continue }
    if ($relative -notmatch '\.(exe|dll|qm)$' -and $relative.Replace('\', '/') -ne 'defaults/manual.md') { continue }
    $target = [IO.Path]::GetFullPath((Join-Path $install $relative))
    $prefix = [IO.Path]::GetFullPath($install).TrimEnd('\') + '\'
    if (!$target.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid update path' }
    $old = Join-Path $backup $relative
    $existed = Test-Path -LiteralPath $target -PathType Leaf
    if ($existed) {
      New-Item -ItemType Directory -Path (Split-Path -Parent $old) -Force | Out-Null
      Copy-Item -LiteralPath $target -Destination $old -ErrorAction Stop
      if ((Get-UpdateFileHash $target) -ne (Get-UpdateFileHash $old)) { throw 'Backup verification failed' }
    }
    $prepared += [PSCustomObject]@{ Source=$file.FullName; Target=$target; Old=$old; Existed=$existed }
  }
  $changed = @()
  try {
    foreach ($item in $prepared) {
      New-Item -ItemType Directory -Path (Split-Path -Parent $item.Target) -Force | Out-Null
      $changed += $item
      Copy-Item -LiteralPath $item.Source -Destination $item.Target -Force -ErrorAction Stop
      if ((Get-UpdateFileHash $item.Source) -ne (Get-UpdateFileHash $item.Target)) { throw 'Install verification failed' }
    }
  } catch {
    $failure = $_
    $rollbackErrors = @()
    foreach ($item in $changed) {
      try {
        if ($item.Existed) { Copy-Item -LiteralPath $item.Old -Destination $item.Target -Force -ErrorAction Stop }
        elseif (Test-Path -LiteralPath $item.Target) { Remove-Item -LiteralPath $item.Target -ErrorAction Stop }
      } catch { $rollbackErrors += $item.Target }
    }
    throw "Update failed: $failure. Original files: $backup. Rollback failures: $($rollbackErrors -join ', ')"
  }
}

try {
  if (Test-Path -LiteralPath $extract) { throw 'Update staging directory already exists' }

  New-Item -ItemType Directory -Path $extract | Out-Null

  $processes = @(Get-TimeTableProcess)

  foreach ($process in $processes) {
    try {
      $process.CloseMainWindow() | Out-Null
    } catch {}
  }

  $updateLock = [IO.File]::Open($updateLockPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
  Wait-TimeTableProcesses -Seconds 20
  Wait-UpdaterExit
  Start-Sleep -Seconds 3
  Add-Type -AssemblyName System.IO.Compression.FileSystem
  [IO.Compression.ZipFile]::ExtractToDirectory($zip, $extract)
  Install-VerifiedFiles
  $updateLock.Dispose()
  $updateLock = $null
  Remove-Item -LiteralPath $updateLockPath
  Start-Process -FilePath (Join-Path $install 'TimeTable.exe') -WorkingDirectory $install
} catch {
  $_ | Out-File -LiteralPath $log -Encoding UTF8
  Start-Process notepad.exe $log
} finally {
  if ($null -ne $updateLock) {
    $updateLock.Dispose()
    Remove-Item -LiteralPath $updateLockPath -ErrorAction SilentlyContinue
  }
}
)PS")
            .arg(
                quotedPowerShellString(zipPath),
                quotedPowerShellString(installDir),
                QString::number(QCoreApplication::applicationPid()));

        // Windows PowerShell 5.1にもUTF-8として読ませる。
        const QByteArray scriptBytes = QByteArray::fromHex("efbbbf") + script.toUtf8();
        if (helperFile.write(scriptBytes) != scriptBytes.size() || !helperFile.commit())
        {
            statusLabel->setText("更新用スクリプトを保存できませんでした。");
            return false;
        }

        QStringList arguments;
        arguments << "-NoProfile"
                  << "-ExecutionPolicy"
                  << "Bypass"
                  << "-File"
                  << helperPath;

        const bool started = QProcess::startDetached(
            "powershell",
            arguments,
            QDir::tempPath());

        if (!started)
        {
            statusLabel->setText("更新用スクリプトを起動できませんでした。");
            detailLabel->setText(helperPath);
        }

        return started;
    }

    QLabel *currentVersionLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *detailLabel = nullptr;
    QProgressBar *progressBar = nullptr;
    QPushButton *checkButton = nullptr;
    QPushButton *updateButton = nullptr;
    QPushButton *releasePageButton = nullptr;
    QNetworkAccessManager network;
    QUrl latestReleasePageUrl;
    QUrl latestDownloadUrl;
    QString latestAssetName;
    qint64 latestAssetSize = 0;
    QString latestAssetDigest;
    QString latestTagName;
};
}

// 更新アプリを初期化して更新画面を表示する
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("TimeTableUpdater");
    QApplication::setApplicationVersion(kCurrentVersion);

    UpdaterDialog dialog;
    dialog.show();

    return QApplication::exec();
}
