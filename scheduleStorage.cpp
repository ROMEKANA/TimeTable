#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStatusBar>

namespace
{
    // 文字列リストを時間割保存用のJSON配列に変換する
    QJsonArray scheduleStringListToJsonArray(const QStringList &values)
    {
        QJsonArray array;

        for (const QString &value : values)
        {
            array.append(value);
        }

        return array;
    }

    // JSONオブジェクト内の配列から文字列リストを取得する
    QStringList scheduleStringListFromJsonArray(
        const QJsonObject &root,
        const QString &key)
    {
        QStringList values;
        const QJsonArray array = root.value(key).toArray();

        for (const QJsonValue &value : array)
        {
            values << value.toString().trimmed();
        }

        return values;
    }

}

namespace
{
    constexpr const char *kScheduleFileExtension = ".schedule"; // 時間割ファイルの拡張子
}

// 現在の時間割を保存用JSONへ変換する
QString MainWindow::scheduleToJson() const
{
    QJsonArray daysArray;

    for (const QVector<TeacherColumn> &day : schedule)
    {
        QJsonArray teachersArray;

        for (const TeacherColumn &teacher : day)
        {
            QJsonObject teacherObject;
            teacherObject["teacherName"] = teacher.teacherName;

            QJsonArray periodsArray;

            for (const QVector<LessonData> &periodLessons : teacher.lessons)
            {
                QJsonArray studentArray;

                for (const LessonData &lesson : periodLessons)
                {
                    const QJsonDocument lessonDocument =
                        QJsonDocument::fromJson(lessonToJson(lesson).toUtf8());

                    studentArray.append(lessonDocument.object());
                }

                periodsArray.append(studentArray);
            }

            teacherObject["lessons"] = periodsArray;
            teachersArray.append(teacherObject);
        }

        daysArray.append(teachersArray);
    }

    QJsonObject root;
    root["version"] = 3;
    root["monday"] = scheduleMonday.toString("yyyy-MM-dd");
    root["days"] = scheduleStringListToJsonArray(days);
    root["periods"] = scheduleStringListToJsonArray(periods);
    root["schedule"] = daysArray;

    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
}

// 保存用JSONを時間割データと見出しへ変換する
bool MainWindow::jsonToScheduleData(
    const QString &json,
    QDate *monday,
    QVector<QVector<TeacherColumn>> *loadedSchedule,
    QStringList *loadedDays,
    QStringList *loadedPeriods) const
{
    if (monday == nullptr || loadedSchedule == nullptr ||
        !DataIntegrity::validDocument(json.toUtf8(), "schedule")) return false;
    const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
    const QStringList fileDays = scheduleStringListFromJsonArray(root, "days");
    const QStringList filePeriods = scheduleStringListFromJsonArray(root, "periods");
    QVector<QVector<TeacherColumn>> parsed;
    for (const QJsonValue &day : root.value("schedule").toArray())
    {
        QVector<TeacherColumn> columns;
        for (const QJsonValue &value : day.toArray())
        {
            const QJsonObject object = value.toObject();
            TeacherColumn teacher;
            teacher.teacherName = object.value("teacherName").toString();
            for (const QJsonValue &period : object.value("lessons").toArray())
            {
                QVector<LessonData> lessons;
                if (period.isArray())
                {
                    for (const QJsonValue &item : period.toArray())
                    {
                        LessonData lesson;
                        if (!jsonToLesson(QString::fromUtf8(QJsonDocument(item.toObject()).toJson()), &lesson)) return false;
                        lessons.append(lesson);
                    }
                }
                else
                {
                    const QJsonObject legacy = period.toObject();
                    LessonData first;
                    if (!jsonToLesson(QString::fromUtf8(QJsonDocument(legacy).toJson()), &first)) return false;
                    lessons.append(first);
                    LessonData second;
                    second.studentName = legacy.value("student2Name").toString();
                    second.studentGrade = legacy.value("student2Grade").toString();
                    second.subject = legacy.value("student2Subject").toString();
                    second.memo = legacy.value("student2Memo").toString();
                    lessons.append(second);
                }
                // 現在の人数設定を超えた実データは切り捨てず、読込を中止する。
                for (int i = MaxStudentPerTeacher; i < lessons.size(); ++i)
                    if (!lessonDataIsEmpty(lessons[i]) || lessons[i].maxStudents != 0) return false;
                lessons.resize(MaxStudentPerTeacher);
                teacher.lessons.append(lessons);
            }
            columns.append(teacher);
        }
        parsed.append(columns);
    }
    *monday = QDate::fromString(root.value("monday").toString(), "yyyy-MM-dd");
    *loadedSchedule = parsed;
    if (loadedDays != nullptr) *loadedDays = fileDays;
    if (loadedPeriods != nullptr) *loadedPeriods = filePeriods;
    return true;
}

// 指定週のファイルを画面へ反映せず時間割データとして読み込む
bool MainWindow::loadScheduleDataFromFile(
    const QDate &monday,
    QDate *fileMonday,
    QVector<QVector<TeacherColumn>> *loadedSchedule,
    QStringList *loadedDays,
    QStringList *loadedPeriods) const
{
    const QDate targetMonday = mondayOf(monday);
    QByteArray bytes;
    if (!readDataFile(scheduleFilePath(targetMonday), &bytes, false)) return false;
    const QString json = QString::fromUtf8(bytes);

    QDate parsedMonday;
    QVector<QVector<TeacherColumn>> parsedSchedule;
    QStringList parsedDays;
    QStringList parsedPeriods;

    if (!jsonToScheduleData(
            json,
            &parsedMonday,
            &parsedSchedule,
            &parsedDays,
            &parsedPeriods))
    {
        return false;
    }

    if (parsedMonday != targetMonday)
    {
        return false;
    }

    if (fileMonday != nullptr)
    {
        *fileMonday = parsedMonday;
    }

    if (loadedSchedule != nullptr)
    {
        *loadedSchedule = parsedSchedule;
    }

    if (loadedDays != nullptr)
    {
        *loadedDays = parsedDays;
    }

    if (loadedPeriods != nullptr)
    {
        *loadedPeriods = parsedPeriods;
    }

    return true;
}

// 読み込んだ曜日と時限の見出しを適用する
void MainWindow::applyScheduleHeaders(
    const QStringList &loadedDays,
    const QStringList &loadedPeriods)
{
    days = loadedDays;
    periods = loadedPeriods;
}

// 保存用JSONを現在の時間割へ反映する
bool MainWindow::jsonToSchedule(const QString &json)
{
    QDate loadedMonday;
    QVector<QVector<TeacherColumn>> loadedSchedule;
    QStringList loadedDays;
    QStringList loadedPeriods;

    if (!jsonToScheduleData(
            json,
            &loadedMonday,
            &loadedSchedule,
            &loadedDays,
            &loadedPeriods))
    {
        return false;
    }

    applyScheduleHeaders(loadedDays, loadedPeriods);
    scheduleMonday = loadedMonday;
    schedule = loadedSchedule;

    renderTable();
    clearCellEditHistory();

    return true;
}

// 起動時に指定された時間割または今週の時間割を開く
void MainWindow::loadLatestSchedule()
{
    if (!startupScheduleFilePath.trimmed().isEmpty())
    {
        const QString filePath = startupScheduleFilePath;
        startupScheduleFilePath.clear();

        if (loadScheduleFromFilePath(filePath))
        {
            ui->mainTabWidget->setCurrentIndex(0);
            statusBar()->showMessage("時間割を読み込みました", 2000);
            return;
        }

        QMessageBox::warning(
            this,
            "読み込みエラー",
            "指定された時間割ファイルを読み込めませんでした。");
    }

    scheduleMonday =
        startupScheduleMonday.isValid()
            ? startupScheduleMonday
            : mondayOf(QDate::currentDate());

    if (!loadScheduleFromFile(scheduleMonday))
    {
        if (QFile::exists(scheduleFilePath(scheduleMonday)))
        {
            scheduleMonday = QDate();
            activeSchedulePath.clear();
            initializeTable();
            renderTable();
            statusBar()->showMessage("時間割を開けませんでした。別の週またはバックアップを確認してください。");
            return;
        }
        activeSchedulePath = scheduleFilePath(scheduleMonday);
        initializeTable();
        renderTable();
        clearCellEditHistory();

        statusBar()->showMessage(
            scheduleMonday.toString("yyyy年M月d日") + "の週を新規作成しました",
            2000);
        return;
    }

    statusBar()->showMessage(
        scheduleMonday.toString("yyyy年M月d日") + "の週を読み込みました",
        2000);
    clearCellEditHistory();
}

// 指定週の時間割ファイルを現在の画面へ読み込む
bool MainWindow::loadScheduleFromFile(const QDate &monday)
{
    const QDate targetMonday = mondayOf(monday);
    QFile file(scheduleFilePath(targetMonday));

    if (!file.exists())
    {
        return false;
    }

    QByteArray bytes;
    if (!readDataFile(scheduleFilePath(targetMonday), &bytes)) return false;
    QDate loadedMonday;
    QVector<QVector<TeacherColumn>> loadedSchedule;
    QStringList loadedDays;
    QStringList loadedPeriods;
    if (!jsonToScheduleData(QString::fromUtf8(bytes), &loadedMonday, &loadedSchedule, &loadedDays, &loadedPeriods) || loadedMonday != targetMonday)
    {
        QMessageBox::warning(this, "読み込みエラー", "時間割の形式・週の日付・最大生徒数の設定が合いません。元ファイルと表示中の時間割を保持します。");
        return false;
    }

    activeSchedulePath = scheduleFilePath(targetMonday);
    applyScheduleHeaders(loadedDays, loadedPeriods);
    scheduleMonday = loadedMonday;
    schedule = loadedSchedule;

    renderTable();
    clearCellEditHistory();

    return true;
}

// 任意のパスにある時間割ファイルを読み込む
bool MainWindow::loadScheduleFromFilePath(const QString &filePath)
{
    QByteArray bytes;
    if (!readDataFile(filePath, &bytes) || !jsonToSchedule(QString::fromUtf8(bytes))) return false;
    activeSchedulePath = QFileInfo(filePath).absoluteFilePath();
    return true;
}

// 未保存内容を確認して表示する週を切り替える
void MainWindow::switchScheduleWeek(const QDate &date)
{
    const QDate targetMonday = mondayOf(date);

    if (!targetMonday.isValid() || targetMonday == scheduleMonday)
    {
        return;
    }

    updateCell();

    if (!confirmClearCellEditHistory("週の切り替え"))
    {
        return;
    }

    if (!confirmSaveScheduleChanges("週の切り替え"))
    {
        return;
    }

    if (QFile::exists(scheduleFilePath(targetMonday)))
    {
        if (!loadScheduleFromFile(targetMonday)) return;
    }
    else
    {
        scheduleMonday = targetMonday;
        activeSchedulePath = scheduleFilePath(targetMonday);
        initializeTable();
        renderTable();
        clearCellEditHistory();

        statusBar()->showMessage(
            scheduleMonday.toString("yyyy年M月d日") + "の週を新規作成しました",
            2000);
        return;
    }

    statusBar()->showMessage(
        scheduleMonday.toString("yyyy年M月d日") + "の週を読み込みました",
        2000);
    clearCellEditHistory();
}

// 指定日を含む週の月曜日を返す
QDate MainWindow::mondayOf(const QDate &date) const
{
    return date.addDays(1 - date.dayOfWeek());
}

// 時間割ファイルの保存フォルダを返す
QString MainWindow::schedulesDirPath() const
{
    return QCoreApplication::applicationDirPath() + "/schedules";
}

// 指定週の時間割ファイルパスを返す
QString MainWindow::scheduleFilePath(const QDate &monday) const
{
    QDir dir(schedulesDirPath());

    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    return dir.filePath(monday.toString("yyyy-MM-dd") + kScheduleFileExtension);
}
