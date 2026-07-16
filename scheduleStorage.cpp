#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStatusBar>

namespace
{
    QJsonArray scheduleStringListToJsonArray(const QStringList &values)
    {
        QJsonArray array;

        for (const QString &value : values)
        {
            array.append(value);
        }

        return array;
    }

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
    constexpr const char *kScheduleFileExtension = ".schedule";
}

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

bool MainWindow::jsonToScheduleData(
    const QString &json,
    QDate *monday,
    QVector<QVector<TeacherColumn>> *loadedSchedule,
    QStringList *loadedDays,
    QStringList *loadedPeriods) const
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject() || monday == nullptr || loadedSchedule == nullptr)
    {
        return false;
    }

    const QJsonObject root = document.object();

    if (!root.contains("monday") || !root.contains("schedule"))
    {
        return false;
    }

    const QDate fileMonday = QDate::fromString(
        root.value("monday").toString(),
        "yyyy-MM-dd");

    if (!fileMonday.isValid())
    {
        return false;
    }

    const QJsonArray daysArray = root.value("schedule").toArray();
    const QStringList fileDays = scheduleStringListFromJsonArray(root, "days");
    const QStringList filePeriods = scheduleStringListFromJsonArray(root, "periods");

    loadedSchedule->clear();

    for (int dayIndex = 0; dayIndex < fileDays.size(); ++dayIndex)
    {
        QVector<TeacherColumn> dayColumns;
        const QJsonArray teachersArray =
            dayIndex < daysArray.size()
                ? daysArray.at(dayIndex).toArray()
                : QJsonArray();

        for (const QJsonValue &teacherValue : teachersArray)
        {
            const QJsonObject teacherObject = teacherValue.toObject();
            TeacherColumn teacher;
            teacher.teacherName = teacherObject.value("teacherName").toString();
            teacher.lessons.clear();
            teacher.lessons.resize(filePeriods.size());

            for (QVector<LessonData> &periodLessons : teacher.lessons)
            {
                periodLessons.resize(MaxStudentPerTeacher);
            }

            const QJsonArray lessonsArray = teacherObject.value("lessons").toArray();

            for (int periodIndex = 0;
                 periodIndex < lessonsArray.size() &&
                 periodIndex < teacher.lessons.size();
                 ++periodIndex)
            {
                const QJsonValue periodValue = lessonsArray.at(periodIndex);

                if (periodValue.isArray())
                {
                    const QJsonArray studentArray = periodValue.toArray();

                    for (int studentIndex = 0;
                         studentIndex < studentArray.size() &&
                         studentIndex < teacher.lessons[periodIndex].size();
                         ++studentIndex)
                    {
                        const QJsonObject lessonObject =
                            studentArray.at(studentIndex).toObject();

                        teacher.lessons[periodIndex][studentIndex] =
                            jsonToLesson(QString::fromUtf8(
                                QJsonDocument(lessonObject).toJson(QJsonDocument::Compact)));
                    }

                    continue;
                }

                // 旧形式: 1時限のオブジェクトに生徒1・2が入っていた。
                const QJsonObject oldLesson = periodValue.toObject();

                if (!teacher.lessons[periodIndex].isEmpty())
                {
                    teacher.lessons[periodIndex][0] =
                        jsonToLesson(QString::fromUtf8(
                            QJsonDocument(oldLesson).toJson(
                                QJsonDocument::Compact)));
                }

                if (teacher.lessons[periodIndex].size() >= 2)
                {
                    LessonData secondLesson;
                    secondLesson.studentName =
                        oldLesson.value("student2Name").toString();
                    secondLesson.studentGrade =
                        oldLesson.value("student2Grade").toString();
                    secondLesson.subject =
                        oldLesson.value("student2Subject").toString();
                    secondLesson.memo =
                        oldLesson.value("student2Memo").toString();

                    teacher.lessons[periodIndex][1] = secondLesson;
                }
            }

            dayColumns.append(teacher);
        }

        if (dayColumns.isEmpty())
        {
            TeacherColumn emptyColumn;
            emptyColumn.lessons.clear();
            emptyColumn.lessons.resize(filePeriods.size());

            for (QVector<LessonData> &periodLessons : emptyColumn.lessons)
            {
                periodLessons.resize(MaxStudentPerTeacher);
            }

            dayColumns.append(emptyColumn);
        }

        loadedSchedule->append(dayColumns);
    }

    *monday = fileMonday;

    if (loadedDays != nullptr)
    {
        *loadedDays = fileDays;
    }

    if (loadedPeriods != nullptr)
    {
        *loadedPeriods = filePeriods;
    }

    return true;
}

bool MainWindow::loadScheduleDataFromFile(
    const QDate &monday,
    QDate *fileMonday,
    QVector<QVector<TeacherColumn>> *loadedSchedule,
    QStringList *loadedDays,
    QStringList *loadedPeriods) const
{
    const QDate targetMonday = mondayOf(monday);
    QFile file(scheduleFilePath(targetMonday));

    if (!file.exists())
    {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    const QString json = QString::fromUtf8(file.readAll());
    file.close();

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

void MainWindow::applyScheduleHeaders(
    const QStringList &loadedDays,
    const QStringList &loadedPeriods)
{
    days = loadedDays;
    periods = loadedPeriods;
}

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

bool MainWindow::loadScheduleFromFile(const QDate &monday)
{
    const QDate targetMonday = mondayOf(monday);
    QFile file(scheduleFilePath(targetMonday));

    if (!file.exists())
    {
        return false;
    }

    QDate loadedMonday;
    QVector<QVector<TeacherColumn>> loadedSchedule;
    QStringList loadedDays;
    QStringList loadedPeriods;

    if (!loadScheduleDataFromFile(
            targetMonday,
            &loadedMonday,
            &loadedSchedule,
            &loadedDays,
            &loadedPeriods))
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "時間割ファイルの形式が正しくありません。");
        return false;
    }

    applyScheduleHeaders(loadedDays, loadedPeriods);
    scheduleMonday = loadedMonday;
    schedule = loadedSchedule;

    renderTable();
    clearCellEditHistory();

    return true;
}

bool MainWindow::loadScheduleFromFilePath(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    const QString json = QString::fromUtf8(file.readAll());
    file.close();

    return jsonToSchedule(json);
}

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

    scheduleMonday = targetMonday;

    if (!loadScheduleFromFile(scheduleMonday))
    {
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

QDate MainWindow::mondayOf(const QDate &date) const
{
    return date.addDays(1 - date.dayOfWeek());
}

QString MainWindow::schedulesDirPath() const
{
    return QCoreApplication::applicationDirPath() + "/schedules";
}

QString MainWindow::scheduleFilePath(const QDate &monday) const
{
    QDir dir(schedulesDirPath());

    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    return dir.filePath(monday.toString("yyyy-MM-dd") + kScheduleFileExtension);
}
