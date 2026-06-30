#include "mainwindow.h"
#include "ui_mainwindow.h"

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
    root["version"] = 2;
    root["monday"] = scheduleMonday.toString("yyyy-MM-dd");
    root["schedule"] = daysArray;

    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool MainWindow::jsonToSchedule(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject())
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

    schedule.clear();
    scheduleMonday = fileMonday;

    for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex)
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
            initializeTeacherLessons(teacher);

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
            initializeTeacherLessons(emptyColumn);
            dayColumns.append(emptyColumn);
        }

        schedule.append(dayColumns);
    }

    renderTable();
    clearCellEditHistory();

    return true;
}

void MainWindow::loadLatestSchedule()
{
    scheduleMonday = mondayOf(QDate::currentDate());

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

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "時間割を読み込めませんでした。");
        return false;
    }

    const QString json = QString::fromUtf8(file.readAll());
    file.close();

    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject())
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "時間割ファイルの形式が正しくありません。");
        return false;
    }

    const QDate fileMonday = QDate::fromString(
        document.object().value("monday").toString(),
        "yyyy-MM-dd");

    if (fileMonday != targetMonday)
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "ファイル名と時間割の月曜日が一致しません。");
        return false;
    }

    return jsonToSchedule(json);
}

void MainWindow::switchScheduleWeek(const QDate &date)
{
    const QDate targetMonday = mondayOf(date);

    if (!targetMonday.isValid() || targetMonday == scheduleMonday)
    {
        return;
    }

    saveScheduleToFile();

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

QString MainWindow::scheduleFilePath(const QDate &monday)
{
    QDir dir(schedulesDirPath());

    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    return dir.filePath(monday.toString("yyyy-MM-dd") + ".json");
}
