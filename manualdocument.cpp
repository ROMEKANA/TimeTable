#include "manualdocument.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>

// 編集用マニュアルの絶対パスを返す
QString manualFilePath()
{
    return QCoreApplication::applicationDirPath() + "/data/manual.md";
}

// 初回コピーとUTF-8読み込みを行い、成功時だけページ一覧を更新する
bool loadManualPages(QVector<ManualPage> *pages, QString *error)
{
    const QString path = manualFilePath();
    if (!QFile::exists(path))
    {
        const QString source = QCoreApplication::applicationDirPath() + "/defaults/manual.md";
        if (!QDir().mkpath(QCoreApplication::applicationDirPath() + "/data") ||
            !QFile::copy(source, path))
        {
            *error = "初期マニュアルをコピーできません。defaults/manual.md の同梱と、dataフォルダーの書き込み権限を確認してください。";
            return false;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error = "マニュアルを開けません：" + file.errorString();
        return false;
    }
    constexpr qint64 maxBytes = 2 * 1024 * 1024;
    const QByteArray bytes = file.read(maxBytes + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() > maxBytes)
    {
        *error = "読み込みに失敗したか、マニュアルが上限の2 MiBを超えています。";
        return false;
    }
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString text = decoder(bytes);
    if (decoder.hasError())
    {
        *error = "文字コードを読み取れません。マニュアルをUTF-8で保存してください。";
        return false;
    }
    return parseManualPages(text, pages, error);
}

// 見出しとコードブロックを区別して目次・検索用本文を作る
bool parseManualPages(const QString &text, QVector<ManualPage> *pages, QString *error)
{
    QVector<ManualPage> parsed;
    QString category;
    QSet<QString> ids;
    QChar fenceCharacter;
    int fenceLength = 0;
    bool inPage = false;
    const QRegularExpression heading("^(#{1,6})[ \\t]+(.+?)\\s*$");
    const QRegularExpression pageId("^(.*?)\\s+\\{([A-Za-z0-9_-]+)\\}$");
    const QRegularExpression fence("^ {0,3}(`{3,}|~{3,})(.*)$");
    int lineNumber = 0;

    for (QString line : text.split('\n'))
    {
        ++lineNumber;
        if (line.endsWith('\r')) line.chop(1);
        if (lineNumber == 1 && line.startsWith(QChar(0xfeff))) line.remove(0, 1);
        const auto fenceMatch = fence.match(line);
        const bool wasInFence = fenceLength > 0;
        if (fenceMatch.hasMatch())
        {
            const QString marker = fenceMatch.captured(1);
            if (!wasInFence)
            {
                fenceCharacter = marker[0];
                fenceLength = marker.size();
            }
            else if (marker[0] == fenceCharacter && marker.size() >= fenceLength &&
                     fenceMatch.captured(2).trimmed().isEmpty())
            {
                fenceLength = 0;
            }
        }
        if (!wasInFence && !fenceMatch.hasMatch())
        {
            const auto match = heading.match(line);
            if (match.hasMatch())
            {
                const int level = match.captured(1).size();
                QString title = match.captured(2).trimmed();
                if (level == 1)
                {
                    category = title;
                    inPage = false;
                    continue;
                }
                if (level == 2)
                {
                    QString id;
                    const auto idMatch = pageId.match(title);
                    if (idMatch.hasMatch())
                    {
                        title = idMatch.captured(1).trimmed();
                        id = idMatch.captured(2);
                    }
                    if (category.isEmpty() || title.isEmpty() || (!id.isEmpty() && ids.contains(id)))
                    {
                        *error = QString("%1行目：ページの前に # グループ名が必要です。ページ名やIDの重複も確認してください。").arg(lineNumber);
                        return false;
                    }
                    if (!id.isEmpty()) ids.insert(id);
                    parsed.append({category, title, id, "# " + title + "\n\n", {}});
                    inPage = true;
                    continue;
                }
                // ファイルの###・####を表示上の大見出し・小見出しにする。
                if (inPage) line.remove(0, 1);
            }
        }
        if (inPage)
        {
            parsed.last().markdown += line + '\n';
        }
        else if (!line.trimmed().isEmpty())
        {
            *error = QString("%1行目：本文は ## ページ名 の後に書いてください。").arg(lineNumber);
            return false;
        }
    }
    if (fenceLength != 0 || parsed.isEmpty())
    {
        *error = "ページがないか、コードブロックが閉じられていません。# グループ名 と ## ページ名 を確認してください。";
        return false;
    }
    QTextDocument document;
    for (ManualPage &page : parsed)
    {
        document.setMarkdown(page.markdown, QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);
        page.searchText = (page.category + ' ' + page.title + ' ' + document.toPlainText()).toCaseFolded();
    }
    *pages = std::move(parsed);
    error->clear();
    return true;
}

// Markdown本文へ設定の文字サイズ・色・行間を適用する
void renderManualPage(QTextDocument *document, const ManualPage &page, const QJsonObject &settings)
{
    const auto number = [&settings](const char *key, int fallback, int min, int max)
    {
        return qBound(min, settings.value(key).toInt(fallback), max);
    };
    const auto color = [&settings](const char *key, const char *fallback)
    {
        const QColor value(settings.value(key).toString(fallback));
        return value.isValid() ? value : QColor(fallback);
    };
    QFont font(settings.value("manualFontFamily").toString("Yu Gothic UI"));
    font.setPointSize(number("manualBodyFontSize", 11, 6, 48));
    document->setDefaultFont(font);
    document->setDocumentMargin(number("manualMargin", 16, 0, 80));
    document->setMarkdown(page.markdown, QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub) | QTextDocument::MarkdownNoHTML);

    const int sizes[] = {font.pointSize(), number("manualTitleFontSize", 22, 6, 72),
                         number("manualHeadingFontSize", 16, 6, 72), number("manualSubheadingFontSize", 13, 6, 72)};
    // 変更中にfragment iteratorを無効化しないよう書式を先に収集する。
    struct Span { int position; int length; QTextCharFormat format; };
    QVector<Span> spans;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
    {
        const int headingLevel = block.blockFormat().headingLevel();
        const bool codeBlock = block.blockFormat().hasProperty(QTextFormat::BlockCodeFence);
        QTextCursor cursor(block);
        QTextBlockFormat format = block.blockFormat();
        format.setLineHeight(number("manualLineHeight", 140, 100, 240), QTextBlockFormat::ProportionalHeight);
        format.setBottomMargin(number("manualParagraphSpacing", 8, 0, 48));
        if (headingLevel > 0) format.setTopMargin(12);
        cursor.setBlockFormat(format);
        for (auto it = block.begin(); !it.atEnd(); ++it)
        {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) continue;
            QTextCharFormat style = fragment.charFormat();
            const bool important = style.fontItalic() && !codeBlock;
            style.setFontPointSize(sizes[qBound(0, headingLevel, 3)]);
            style.setForeground(color(headingLevel > 0 ? "manualHeadingColor" : "manualTextColor",
                                      headingLevel > 0 ? "#244765" : "#202020"));
            if (style.isAnchor()) style.setForeground(color("manualLinkColor", "#1565c0"));
            if (important)
            {
                style.setFontItalic(false);
                style.setForeground(color("manualImportantColor", "#c62828"));
            }
            spans.append({fragment.position(), fragment.length(), style});
        }
    }
    for (const Span &span : spans)
    {
        QTextCursor cursor(document);
        cursor.setPosition(span.position);
        cursor.setPosition(span.position + span.length, QTextCursor::KeepAnchor);
        cursor.setCharFormat(span.format);
    }
}
