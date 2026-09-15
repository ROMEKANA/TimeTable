#ifndef DATAINTEGRITY_H
#define DATAINTEGRITY_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>

namespace DataIntegrity
{
// 新旧の授業JSONを検証する。空授業と不正入力を区別する。
bool validLesson(const QJsonObject &object);
// 保存対象のJSON構造・型・配列の整合性を検証する。
bool validDocument(const QByteArray &bytes, const QString &kind);
// 一時ファイルへ全量を書き、成功時だけ置き換える。
bool atomicWrite(const QString &path, const QByteArray &bytes, QString *error);
// 指定パスが基準フォルダー内にあるか判定する。
bool isPathInsideDirectory(const QString &path, const QString &directory);
}

class SafeStorage
{
public:
    // 保存元を基準にバックアップの配置先を決める。
    explicit SafeStorage(const QString &baseDirectory);
    // 読み込み前のバックアップと検証が成功したときだけ内容を返す。
    bool read(const QString &path, QByteArray *bytes, QString *error, bool updateBaseline = true);
    // 読込失敗・外部変更を拒否し、上書き前に退避してから保存する。
    bool write(const QString &path, const QByteArray &bytes, QString *error);
    // バックアップからの復元先を退避し、安全に置換できる状態へする。
    bool prepareRestoreTarget(const QString &path, QString *error);
    // 構造変更前の未保存データを世代バックアップへ保存する。
    bool snapshot(const QString &path, const QByteArray &bytes, QString *error) const;
    // バックアップフォルダーの絶対パスを返す。
    QString backupDirectory() const;

private:
    // 元ファイルの配置を保ち、外部ファイルはパスのハッシュで区別する。
    QString backupRelativePath(const QString &path) const;
    QString baseDirectory;
    QHash<QString, QByteArray> fingerprints;
    QSet<QString> missingFiles;
    QSet<QString> blockedFiles;
};

#endif
