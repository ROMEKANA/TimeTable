#include "dataIntegrity.h"

#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTextStream>

namespace
{
int checks = 0;
int failures = 0;
// 失敗を集計し、常にテスト名を表示する。
void check(bool passed, const char *name)
{
    ++checks;
    if (!passed) { ++failures; QTextStream(stderr) << "FAIL: " << name << '\n'; }
}
// テスト専用ファイルの内容を取得する。
QByteArray contents(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
// 実アプリのファイルに触れず、試験用データを配置する。
void put(const QString &path, const QByteArray &bytes)
{
    QString error;
    check(DataIntegrity::atomicWrite(path, bytes, &error), "fixture write");
}
// 最小の有効な時間割を作成する。
QJsonObject scheduleFixture()
{
    QJsonObject lesson{{"studentName", "A"}, {"studentGrade", "G"}, {"subject", "S"}, {"memo", "M"}, {"maxStudents", 0}};
    QJsonObject teacher{{"teacherName", "T"}, {"lessons", QJsonArray{QJsonArray{lesson}}}};
    return {{"version", 3}, {"monday", "2026-09-07"}, {"days", QJsonArray{"月"}},
            {"periods", QJsonArray{"1限目"}}, {"schedule", QJsonArray{QJsonArray{teacher}}}};
}
}

// 保存失敗・破損・外部変更・バックアップ・不正入力を一時領域だけで検証する。
int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "temporary directory");
    if (!directory.isValid()) return 1;
    const QString root = directory.path();
    const QString path = root + "/data/students.json";
    const QByteArray original = R"({"version":2,"gradeStudents":[]})";
    const QByteArray updated = R"({"version":2,"gradeStudents":[{"grade":"G","students":[{"name":"A"}]}]})";
    SafeStorage storage(root);
    QString error;
    QByteArray bytes("unchanged");
    check(!storage.read(path, &bytes, &error) && bytes == "unchanged", "missing read leaves output alone");
    check(storage.write(path, original, &error), "new file save");
    check(storage.read(path, &bytes, &error) && bytes == original, "valid read");
    const QString daily = root + "/backups/" + QDate::currentDate().toString("yyyy-MM-dd");
    check(contents(daily + "/data/students.json") == original, "daily backup matches exact original bytes");
    check(storage.write(path, updated, &error), "atomic overwrite");
    check(contents(path) == updated, "new contents committed");
    check(storage.read(path, &bytes, &error), "second read");
    check(contents(daily + "/data/students.json") == updated, "same day backup updated");
    bool retainedOriginal = false;
    QDirIterator versions(daily + "/versions", QDir::Files, QDirIterator::Subdirectories);
    while (versions.hasNext()) if (contents(versions.next()) == original) retainedOriginal = true;
    check(retainedOriginal, "earlier backup generation retained");
    put(path, original);
    check(!storage.write(path, updated, &error) && contents(path) == original, "external edit blocks overwrite");
    check(storage.read(path, &bytes, &error, false), "background read succeeds");
    check(!storage.write(path, updated, &error), "background read does not reset conflict baseline");
    check(storage.read(path, &bytes, &error), "explicit reload accepts external edit");
    check(storage.write(path, updated, &error), "save after reload");
    QLockFile lock(path + ".lock");
    check(lock.tryLock(), "fixture lock");
    check(!storage.write(path, original, &error) && contents(path) == updated, "concurrent save refuses overwrite");
    lock.unlock();
    check(QFile::remove(path), "fixture delete");
    check(!storage.write(path, original, &error) && !QFile::exists(path), "external deletion blocks recreate");
    put(path, "{}");
    bytes = "unchanged";
    check(!storage.read(path, &bytes, &error) && bytes == "unchanged", "invalid schema leaves loaded state alone");
    check(contents(daily + "/data/students.json") == "{}", "corrupt bytes backed up");
    check(!storage.write(path, original, &error) && contents(path) == "{}", "corrupt original cannot be overwritten");
    check(storage.prepareRestoreTarget(path, &error), "corrupt restore target prepared after backup");
    check(storage.write(path, original, &error) && contents(path) == original, "valid backup can replace corrupt restore target");

    check(DataIntegrity::isPathInsideDirectory(daily + "/data/students.json", root + "/backups"), "backup path detected");
    check(!DataIntegrity::isPathInsideDirectory(root + "/backups-copy/students.json", root + "/backups"), "backup sibling path rejected");

    const QString failureRoot = root + "/backup-failure";
    const QString failurePath = failureRoot + "/data/students.json";
    SafeStorage failing(failureRoot);
    check(failing.write(failurePath, original, &error), "failure fixture new file");
    put(failureRoot + "/backups", "not a directory");
    check(!failing.write(failurePath, updated, &error) && contents(failurePath) == original, "backup failure aborts save");
    bytes = "unchanged";
    check(!failing.read(failurePath, &bytes, &error) && bytes == "unchanged", "backup failure aborts load");
    check(QDir().mkpath(root + "/directory-target"), "fixture target directory");
    check(!DataIntegrity::atomicWrite(root + "/directory-target", "x", &error), "commit to directory fails");
    check(QFileInfo(root + "/directory-target").isDir(), "failed commit preserves destination");

    check(!DataIntegrity::validLesson({}), "unrelated clipboard object rejected");
    check(!DataIntegrity::validLesson({{"studentName", 4}}), "wrong clipboard field type rejected");
    const QJsonObject emptyLesson{{"studentName", ""}, {"studentGrade", ""}, {"subject", ""}, {"memo", ""}};
    check(DataIntegrity::validLesson(emptyLesson), "intentional empty lesson accepted");
    check(DataIntegrity::validLesson({{"student1Name", "A"}, {"student1Grade", "G"}, {"student1Subject", "S"}, {"student1Memo", ""}}), "legacy clipboard accepted");
    // 全127通りの曜日選択で、新旧形式の境界と日付対応を確認する。
    const QStringList calendarDays = {"月", "火", "水", "木", "金", "土", "日"};
    const QJsonArray fixtureColumns = scheduleFixture().value("schedule").toArray();
    for (int mask = 1; mask < 128; ++mask)
    {
        QStringList selected;
        QJsonArray headers;
        QJsonArray columns;
        for (int offset = 0; offset < 7; ++offset)
        {
            if (!(mask & (1 << offset))) continue;
            selected.append(calendarDays[offset]);
            headers.append(calendarDays[offset]);
            columns.append(fixtureColumns.at(0));
            check(DataIntegrity::calendarDayOffset(selected.last()) == offset, "weekday keeps calendar offset");
            check(QDate(2026, 12, 28).addDays(DataIntegrity::calendarDayOffset(selected.last())).dayOfWeek() == offset + 1,
                  "weekday date remains correct across year boundary");
        }
        QJsonObject subset = scheduleFixture();
        subset["days"] = headers;
        subset["schedule"] = columns;
        for (int version = 1; version <= 4; ++version)
        {
            subset["version"] = version;
            const bool expected = version == 4 || selected == calendarDays.mid(0, selected.size());
            check(DataIntegrity::validDocument(QJsonDocument(subset).toJson(), "schedule") == expected,
                  "v1-v3 contiguous and v4 sparse weekday compatibility");
        }
        subset.remove("version");
        check(DataIntegrity::validDocument(QJsonDocument(subset).toJson(), "schedule") ==
                  (selected == calendarDays.mid(0, selected.size())), "unversioned schedule keeps legacy rules");
    }
    for (const QStringList &invalid : QVector<QStringList>{{}, {"月", "月"}, {"水", "月"}, {"月曜日"}, {"祝"}, {" 月"}})
        check(!DataIntegrity::validScheduleDays(invalid), "invalid weekday selection rejected");
    check(DataIntegrity::calendarDayOffset("unknown") == -1, "unknown weekday has no calendar offset");
    QJsonObject sparse = scheduleFixture();
    sparse["version"] = 4;
    sparse["days"] = QJsonArray{"水", "日"};
    sparse["schedule"] = QJsonArray{fixtureColumns.at(0), fixtureColumns.at(0)};
    const QString sparsePath = root + "/schedules/2026-09-07.schedule";
    const QByteArray sparseBytes = QJsonDocument(sparse).toJson();
    check(storage.write(sparsePath, sparseBytes, &error), "v4 sparse schedule saves through SafeStorage");
    check(storage.read(sparsePath, &bytes, &error) && bytes == sparseBytes, "v4 sparse schedule round trip preserves contents");
    sparse["version"] = 5;
    check(!DataIntegrity::validDocument(QJsonDocument(sparse).toJson(), "schedule"), "future schedule version rejected");
    sparse["version"] = 4;
    for (const QJsonArray &invalid : QVector<QJsonArray>{{}, {"水", "水"}, {"日", "水"}, {"水", "祝"}, {"水", 5}})
    {
        sparse["days"] = invalid;
        check(!DataIntegrity::validDocument(QJsonDocument(sparse).toJson(), "schedule"), "invalid v4 weekday headers rejected");
    }

    QJsonObject schedule = scheduleFixture();
    check(DataIntegrity::validDocument(QJsonDocument(schedule).toJson(), "schedule"), "valid schedule accepted");
    schedule["monday"] = "2026-09-08";
    check(!DataIntegrity::validDocument(QJsonDocument(schedule).toJson(), "schedule"), "non Monday date rejected");
    schedule = scheduleFixture(); schedule["days"] = QJsonArray{"月", "火"};
    check(!DataIntegrity::validDocument(QJsonDocument(schedule).toJson(), "schedule"), "dimension mismatch rejected");
    schedule = scheduleFixture(); schedule["version"] = 999;
    check(!DataIntegrity::validDocument(QJsonDocument(schedule).toJson(), "schedule"), "future version rejected");
    schedule = scheduleFixture(); schedule["schedule"] = QJsonArray{QJsonArray{QJsonObject{{"teacherName", "T"}, {"lessons", QJsonArray{QJsonArray{42}}}}}};
    check(!DataIntegrity::validDocument(QJsonDocument(schedule).toJson(), "schedule"), "invalid nested lesson rejected");
    check(!DataIntegrity::validDocument("{", "master"), "truncated master rejected");
    check(!DataIntegrity::validDocument("{}", "master"), "empty master rejected");
    check(!DataIntegrity::validDocument(R"({"teachers":[{"name":4}]})", "teachers"), "invalid teacher rejected");
    check(!DataIntegrity::validDocument(R"({"schools":[42]})", "school"), "invalid school rejected");
    check(!DataIntegrity::validDocument(R"({"gradeStudents":[{"grade":"G","students":[{"name":"A","subjects":[{"subjectName":"S","materials":[42]}]}]}]})", "students"), "invalid nested student data rejected");
    QTextStream(stdout) << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
