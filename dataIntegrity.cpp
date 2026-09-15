#include "dataIntegrity.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <cmath>

namespace
{
// 比較・世代名に使う内容のハッシュを返す。
QByteArray digest(const QByteArray &bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

// ファイル名から検証対象を判定する。
QString documentKind(const QString &path)
{
    const QFileInfo info(path);
    return info.suffix().compare("schedule", Qt::CaseInsensitive) == 0
        ? QString("schedule") : info.completeBaseName();
}

// 整数の型と範囲を確認する。
bool integer(const QJsonValue &value, int minimum, int maximum)
{
    const double number = value.toDouble(-1);
    return value.isDouble() && std::isfinite(number) && number == std::floor(number)
        && number >= minimum && number <= maximum;
}

// 文字列配列の型・空欄・重複を確認する。
bool strings(const QJsonValue &value, bool requireNames = false)
{
    if (!value.isArray() || (requireNames && value.toArray().isEmpty())) return false;
    QSet<QString> names;
    for (const QJsonValue &item : value.toArray())
    {
        if (!item.isString()) return false;
        const QString name = item.toString().trimmed();
        if (requireNames && (name.isEmpty() || names.contains(name))) return false;
        names.insert(name);
    }
    return true;
}

// 任意の文字列項目も、存在する場合は型を検証する。
bool textFields(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys)
        if (object.contains(key) && !object.value(key).isString()) return false;
    return true;
}
}

bool DataIntegrity::validLesson(const QJsonObject &object)
{
    const bool modern = object.contains("studentName");
    const QStringList required = modern
        ? QStringList{"studentName", "studentGrade", "subject", "memo"}
        : QStringList{"student1Name", "student1Grade", "student1Subject", "student1Memo"};
    for (const QString &key : required)
        if (!object.value(key).isString()) return false;
    if (!textFields(object, {"student2Name", "student2Grade", "student2Subject", "student2Memo"})) return false;
    return !object.contains("maxStudents") || integer(object.value("maxStudents"), 0, 100);
}

bool DataIntegrity::validDocument(const QByteArray &bytes, const QString &kind)
{
    const QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (!document.isObject()) return false;
    const QJsonObject root = document.object();
    if (root.contains("version") && !integer(root.value("version"), 1, kind == "schedule" ? 3 : 2)) return false;
    if (kind == "schedule")
    {
        const QDate monday = QDate::fromString(root.value("monday").toString(), "yyyy-MM-dd");
        if (!monday.isValid() || monday.dayOfWeek() != 1 ||
            !strings(root.value("days"), true) || !strings(root.value("periods"), true) ||
            !root.value("schedule").isArray()) return false;
        const QStringList calendarDays = {"月", "火", "水", "木", "金", "土", "日"};
        const QJsonArray headers = root.value("days").toArray();
        if (headers.size() > calendarDays.size()) return false;
        for (int i = 0; i < headers.size(); ++i)
            if (headers.at(i).toString() != calendarDays[i]) return false;
        const QJsonArray days = root.value("schedule").toArray();
        const int periodCount = root.value("periods").toArray().size();
        if (days.size() != root.value("days").toArray().size() || days.size() > 7 || periodCount > 100) return false;
        for (const QJsonValue &day : days)
        {
            if (!day.isArray() || day.toArray().isEmpty() || day.toArray().size() > 1000) return false;
            for (const QJsonValue &teacher : day.toArray())
            {
                if (!teacher.isObject()) return false;
                const QJsonObject column = teacher.toObject();
                if (!column.value("teacherName").isString() || !column.value("lessons").isArray() ||
                    column.value("lessons").toArray().size() != periodCount) return false;
                for (const QJsonValue &period : column.value("lessons").toArray())
                {
                    if (period.isObject())
                    {
                        if (!validLesson(period.toObject())) return false;
                    }
                    else if (period.isArray() && !period.toArray().isEmpty() && period.toArray().size() <= 100)
                    {
                        for (const QJsonValue &lesson : period.toArray())
                            if (!lesson.isObject() || !validLesson(lesson.toObject())) return false;
                    }
                    else return false;
                }
            }
        }
        return true;
    }
    if (kind == "master")
    {
        // 旧版で欠けていた項目は補完できるが、型の違う値は補完で消さない。
        for (auto it = root.begin(); it != root.end(); ++it)
        {
            if (QStringList{"days", "periods", "grades", "genders", "subjects"}.contains(it.key()))
            {
                if (!strings(it.value(), true)) return false;
            }
            else if (!it.value().isString() && !it.value().isDouble() && !it.value().isBool()) return false;
        }
        return !root.isEmpty();
    }
    if (kind == "school") return strings(root.value("schools"), true) ||
        (root.value("schools").isArray() && root.value("schools").toArray().isEmpty());
    if (kind == "teachers")
    {
        if (!root.value("teachers").isArray()) return false;
        QSet<QString> teacherNames;
        for (const QJsonValue &value : root.value("teachers").toArray())
        {
            if (!value.isObject()) return false;
            const QJsonObject teacher = value.toObject();
            if (!teacher.value("name").isString() || teacher.value("name").toString().trimmed().isEmpty() ||
                !textFields(teacher, {"memo"})) return false;
            const QString name = teacher.value("name").toString().trimmed();
            if (teacherNames.contains(name)) return false;
            teacherNames.insert(name);
            for (const QString &key : {"oneOnTwoRate", "oneOnOneRate", "transportPay", "highSchoolAllowance"})
                if (teacher.contains(key) && !integer(teacher.value(key), 0, 999999)) return false;
        }
        return true;
    }
    if (kind == "students")
    {
        if (!root.value("gradeStudents").isArray()) return false;
        QSet<QString> studentNames;
        for (const QJsonValue &groupValue : root.value("gradeStudents").toArray())
        {
            if (!groupValue.isObject()) return false;
            const QJsonObject group = groupValue.toObject();
            if (!group.value("grade").isString() || group.value("grade").toString().trimmed().isEmpty() ||
                !group.value("students").isArray()) return false;
            for (const QJsonValue &value : group.value("students").toArray())
            {
                if (!value.isObject()) return false;
                const QJsonObject student = value.toObject();
                if (!student.value("name").isString() || student.value("name").toString().trimmed().isEmpty() ||
                    !textFields(student, {"memo", "school"})) return false;
                const QString name = QString::fromUtf8(QJsonDocument(QJsonArray{group.value("grade"), student.value("name")}).toJson(QJsonDocument::Compact));
                if (studentNames.contains(name)) return false;
                studentNames.insert(name);
                for (const QString &key : {"grade", "gender"})
                    if (student.contains(key) && !integer(student.value(key), 0, 1000)) return false;
                if (student.contains("subjects") && !student.value("subjects").isArray()) return false;
                for (const QJsonValue &subject : student.value("subjects").toArray())
                {
                    if (subject.isString() && !subject.toString().trimmed().isEmpty()) continue;
                    if (!subject.isObject()) return false;
                    const QJsonObject item = subject.toObject();
                    const QJsonValue name = item.value(item.contains("subjectName") ? "subjectName" : "name");
                    if (!name.isString() || name.toString().trimmed().isEmpty() ||
                        (item.contains("materials") && !strings(item.value("materials")))) return false;
                }
            }
        }
        return true;
    }
    return kind == "appState";
}

bool DataIntegrity::atomicWrite(const QString &path, const QByteArray &bytes, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    {
        *error = "保存先フォルダーを作成できません: " + path;
        return false;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
    {
        *error = "ファイルを保存できません: " + path + "\n" + file.errorString();
        return false;
    }
    return true;
}

bool DataIntegrity::isPathInsideDirectory(const QString &path, const QString &directory)
{
    const QString relative = QDir(QFileInfo(directory).absoluteFilePath())
                                 .relativeFilePath(QFileInfo(path).absoluteFilePath());
    return relative != ".." && !relative.startsWith("../") && !QDir::isAbsolutePath(relative);
}

SafeStorage::SafeStorage(const QString &baseDirectory)
    : baseDirectory(QDir(baseDirectory).absolutePath())
{
}

bool SafeStorage::read(const QString &path, QByteArray *bytes, QString *error, bool updateBaseline)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QFile file(absolute);
    if (!file.exists())
    {
        missingFiles.insert(absolute);
        *error = "ファイルがありません: " + absolute;
        return false;
    }
    blockedFiles.insert(absolute);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error = "読み込めません: " + absolute + "\n" + file.errorString();
        return false;
    }
    const QByteArray loaded = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        *error = "読み込み中にエラーが発生しました: " + absolute;
        return false;
    }
    const QString daily = backupDirectory() + "/" + QDate::currentDate().toString("yyyy-MM-dd");
    // 日別コピーの更新で以前の内容を失わないよう世代も残す。
    if (!snapshot(absolute, loaded, error) ||
        !DataIntegrity::atomicWrite(daily + "/" + backupRelativePath(absolute), loaded, error)) return false;
    if (!DataIntegrity::validDocument(loaded, documentKind(absolute)))
    {
        *error = "データの形式が正しくありません。元ファイルを保持し、バックアップへ退避しました。\n" + absolute;
        return false;
    }
    if (updateBaseline) fingerprints[absolute] = digest(loaded);
    missingFiles.remove(absolute);
    blockedFiles.remove(absolute);
    *bytes = loaded;
    return true;
}

bool SafeStorage::write(const QString &path, const QByteArray &bytes, QString *error)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (blockedFiles.contains(absolute) || !DataIntegrity::validDocument(bytes, documentKind(absolute)))
    {
        *error = "不正なデータまたは読み込みに失敗したファイルは上書きできません。\n" + absolute;
        return false;
    }
    if (!QDir().mkpath(QFileInfo(absolute).absolutePath()))
    {
        *error = "保存先フォルダーを作成できません: " + absolute;
        return false;
    }
    QLockFile lock(absolute + ".lock");
    if (!lock.tryLock(0))
    {
        *error = "別の保存処理がこのファイルを使用しています: " + absolute;
        return false;
    }
    QFile original(absolute);
    if (original.exists())
    {
        if (!original.open(QIODevice::ReadOnly))
        {
            *error = "上書き前のファイルを読み込めません: " + absolute;
            return false;
        }
        const QByteArray old = original.readAll();
        if (original.error() != QFileDevice::NoError || missingFiles.contains(absolute) ||
            !fingerprints.contains(absolute) || fingerprints.value(absolute) != digest(old))
        {
            *error = "保存先が未読込、または読み込み後に外部で変更されています。再読み込みして確認してください。\n" + absolute;
            return false;
        }
        original.close();
        if (old == bytes) return true;
        if (!snapshot(absolute, old, error)) return false;
    }
    else if (fingerprints.contains(absolute))
    {
        *error = "読み込み後にファイルが削除されています。保存を中止しました。\n" + absolute;
        return false;
    }
    if (!DataIntegrity::atomicWrite(absolute, bytes, error)) return false;
    fingerprints[absolute] = digest(bytes);
    missingFiles.remove(absolute);
    return true;
}

bool SafeStorage::prepareRestoreTarget(const QString &path, QString *error)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    QFile original(absolute);
    if (!original.exists())
    {
        fingerprints.remove(absolute);
        missingFiles.remove(absolute);
        blockedFiles.remove(absolute);
        return true;
    }
    if (!original.open(QIODevice::ReadOnly))
    {
        *error = "復元先のファイルを読み込めません: " + absolute + "\n" + original.errorString();
        return false;
    }
    const QByteArray old = original.readAll();
    if (original.error() != QFileDevice::NoError)
    {
        *error = "復元先の読み込み中にエラーが発生しました: " + absolute;
        return false;
    }
    if (!snapshot(absolute, old, error)) return false;
    fingerprints[absolute] = digest(old);
    missingFiles.remove(absolute);
    blockedFiles.remove(absolute);
    return true;
}

bool SafeStorage::snapshot(const QString &path, const QByteArray &bytes, QString *error) const
{
    const QString relative = backupRelativePath(path);
    const QFileInfo info(relative);
    const QString destination = backupDirectory() + "/" + QDate::currentDate().toString("yyyy-MM-dd") +
        "/versions/" + info.path() + "/" + info.completeBaseName() + "-" +
        QString::fromLatin1(digest(bytes)) + "." + info.suffix();
    if (QFileInfo::exists(destination))
    {
        QFile existing(destination);
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes && existing.error() == QFileDevice::NoError) return true;
        *error = "既存バックアップを検証できません: " + destination;
        return false;
    }
    return DataIntegrity::atomicWrite(destination, bytes, error);
}

QString SafeStorage::backupDirectory() const
{
    return baseDirectory + "/backups";
}

QString SafeStorage::backupRelativePath(const QString &path) const
{
    const QString relative = QDir(baseDirectory).relativeFilePath(QFileInfo(path).absoluteFilePath());
    if (!relative.startsWith("../") && !QDir::isAbsolutePath(relative) && !relative.startsWith("backups/")) return relative;
    return "external/" + QString::fromLatin1(digest(QFileInfo(path).absoluteFilePath().toUtf8())) + "/" + QFileInfo(path).fileName();
}
