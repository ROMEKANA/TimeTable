#ifndef MANUALDOCUMENT_H
#define MANUALDOCUMENT_H

#include <QJsonObject>
#include <QString>
#include <QVector>

class QTextDocument;

struct ManualPage
{
    QString category;
    QString title;
    QString id;
    QString markdown;
    QString searchText;
};

QString manualFilePath(); // 編集用マニュアルの絶対パスを返す
bool loadManualPages(QVector<ManualPage> *pages, QString *error); // 初回コピーとUTF-8読み込みを行い、成功時だけページ一覧を更新する
bool parseManualPages(const QString &text, QVector<ManualPage> *pages, QString *error); // 見出しとコードブロックを区別して目次・検索用本文を作る
void renderManualPage(QTextDocument *document, const ManualPage &page, const QJsonObject &settings); // Markdown本文へ設定の文字サイズ・色・行間を適用する

#endif // MANUALDOCUMENT_H
