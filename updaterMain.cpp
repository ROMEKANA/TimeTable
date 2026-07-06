#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
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

QString releaseDateText(const QString &publishedAt)
{
    const QDateTime dateTime = QDateTime::fromString(publishedAt, Qt::ISODate);

    if (!dateTime.isValid())
    {
        return QString();
    }

    return dateTime.toLocalTime().toString("yyyy/MM/dd HH:mm");
}

QString quotedPowerShellString(QString text)
{
    text.replace('\'', "''");
    return QString("'%1'").arg(text);
}

class UpdaterDialog : public QDialog
{
public:
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
                if (latestReleasePageUrl.isValid())
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
        releasePageButton->setEnabled(latestReleasePageUrl.isValid());

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
            latestAssetName = selectedAsset.value("name").toString().trimmed();
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
            updateButton->setEnabled(latestDownloadUrl.isValid());
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

            if (latestDownloadUrl.isValid())
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

    void downloadAndInstallLatestRelease()
    {
        if (!latestDownloadUrl.isValid())
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

    void handleDownloadReply(QNetworkReply *reply)
    {
        if (reply->error() != QNetworkReply::NoError)
        {
            statusLabel->setText("更新ファイルのダウンロードに失敗しました。");
            detailLabel->setText(reply->errorString());
            checkButton->setEnabled(true);
            updateButton->setEnabled(latestDownloadUrl.isValid());
            releasePageButton->setEnabled(latestReleasePageUrl.isValid());
            progressBar->setVisible(false);
            return;
        }

        const QString tempRoot =
            QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString updateDir =
            QDir(tempRoot).filePath("TimeTableUpdate-" + QUuid::createUuid().toString(QUuid::Id128));

        if (!QDir().mkpath(updateDir))
        {
            statusLabel->setText("更新用の一時フォルダを作成できませんでした。");
            return;
        }

        const QString assetName =
            latestAssetName.isEmpty() ? QString("TimeTable-update.zip") : latestAssetName;
        const QString zipPath = QDir(updateDir).filePath(assetName);
        QFile zipFile(zipPath);

        if (!zipFile.open(QIODevice::WriteOnly))
        {
            statusLabel->setText("更新ファイルを保存できませんでした。");
            detailLabel->setText(zipFile.errorString());
            return;
        }

        zipFile.write(reply->readAll());
        zipFile.close();

        if (!startInstallHelper(zipPath))
        {
            checkButton->setEnabled(true);
            updateButton->setEnabled(latestDownloadUrl.isValid());
            releasePageButton->setEnabled(latestReleasePageUrl.isValid());
            return;
        }

        statusLabel->setText("更新を開始します。Updater を終了してファイルを置き換えます...");
        detailLabel->setText("置き換え完了後、TimeTable を自動で起動します。");
        QTimer::singleShot(800, qApp, &QCoreApplication::quit);
    }

    bool startInstallHelper(const QString &zipPath)
    {
        const QString installDir = QCoreApplication::applicationDirPath();
        const QString helperPath =
            QFileInfo(zipPath).absoluteDir().filePath("install-update.ps1");
        QFile helperFile(helperPath);

        if (!helperFile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            statusLabel->setText("更新用スクリプトを作成できませんでした。");
            detailLabel->setText(helperFile.errorString());
            return false;
        }

        const QString script = QString(
            "$ErrorActionPreference = 'Stop'\n"
            "$zip = %1\n"
            "$install = %2\n"
            "$updaterPid = %3\n"
            "$extract = Join-Path (Split-Path -Parent $zip) 'extract'\n"
            "$log = Join-Path (Split-Path -Parent $zip) 'update-error.log'\n"
            "try {\n"
            "  if (Test-Path -LiteralPath $extract) { Remove-Item -LiteralPath $extract -Recurse -Force }\n"
            "  New-Item -ItemType Directory -Path $extract | Out-Null\n"
            "  Get-Process -Name TimeTable -ErrorAction SilentlyContinue | Where-Object { $_.Id -ne $updaterPid } | ForEach-Object { try { $_.CloseMainWindow() | Out-Null } catch {} }\n"
            "  Start-Sleep -Seconds 2\n"
            "  Get-Process -Name TimeTable -ErrorAction SilentlyContinue | Where-Object { $_.Id -ne $updaterPid } | Stop-Process -Force -ErrorAction SilentlyContinue\n"
            "  Wait-Process -Id $updaterPid -ErrorAction SilentlyContinue\n"
            "  Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force\n"
            "  Get-ChildItem -LiteralPath $extract -Force | Copy-Item -Destination $install -Recurse -Force\n"
            "  Start-Process -FilePath (Join-Path $install 'TimeTable.exe') -WorkingDirectory $install\n"
            "} catch {\n"
            "  $_ | Out-File -LiteralPath $log -Encoding UTF8\n"
            "  Start-Process notepad.exe $log\n"
            "}\n")
            .arg(
                quotedPowerShellString(zipPath),
                quotedPowerShellString(installDir),
                QString::number(QCoreApplication::applicationPid()));

        helperFile.write(script.toUtf8());
        helperFile.close();

        QStringList arguments;
        arguments << "-NoProfile"
                  << "-ExecutionPolicy"
                  << "Bypass"
                  << "-File"
                  << helperPath;

        const bool started = QProcess::startDetached("powershell", arguments);

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
    QString latestTagName;
};
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("TimeTableUpdater");
    QApplication::setApplicationVersion(kCurrentVersion);

    UpdaterDialog dialog;
    dialog.show();

    return QApplication::exec();
}
