#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
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

        auto *buttonLayout = new QHBoxLayout();
        checkButton = new QPushButton("再確認", this);
        releasePageButton = new QPushButton("リリースページを開く", this);
        auto *closeButton = new QPushButton("閉じる", this);

        releasePageButton->setEnabled(false);

        buttonLayout->addWidget(checkButton);
        buttonLayout->addWidget(releasePageButton);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);

        layout->addWidget(titleLabel);
        layout->addWidget(currentVersionLabel);
        layout->addWidget(statusLabel);
        layout->addWidget(detailLabel, 1);
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
        releasePageButton->setEnabled(false);
        latestReleasePageUrl = QUrl();
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

        latestReleasePageUrl = QUrl(htmlUrl);
        releasePageButton->setEnabled(latestReleasePageUrl.isValid());

        const QVersionNumber currentVersion = versionFromTag(kCurrentVersion);
        const QVersionNumber latestVersion = versionFromTag(tagName);
        const int comparison = QVersionNumber::compare(latestVersion, currentVersion);

        if (tagName.isEmpty() || latestVersion.isNull())
        {
            statusLabel->setText("最新リリースのバージョンを判定できませんでした。");
        }
        else if (comparison > 0)
        {
            statusLabel->setText(
                QString("新しいバージョンがあります: %1 -> %2")
                    .arg(kCurrentVersion, tagName));
        }
        else
        {
            statusLabel->setText(
                QString("最新版です: %1").arg(kCurrentVersion));
        }

        QString assetText = "配布ファイル: なし";

        if (!assets.isEmpty())
        {
            const QJsonObject asset = assets.first().toObject();
            const QString assetName = asset.value("name").toString().trimmed();
            const QString downloadUrl =
                asset.value("browser_download_url").toString().trimmed();

            if (!assetName.isEmpty())
            {
                assetText = QString("配布ファイル: %1").arg(assetName);
            }

            if (!downloadUrl.isEmpty())
            {
                assetText += QString("\nダウンロードURL: %1").arg(downloadUrl);
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

    QLabel *currentVersionLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *detailLabel = nullptr;
    QPushButton *checkButton = nullptr;
    QPushButton *releasePageButton = nullptr;
    QNetworkAccessManager network;
    QUrl latestReleasePageUrl;
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
