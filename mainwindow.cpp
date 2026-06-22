#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("塾時間割表");
    //resize(1500, 760);

    days = {"月", "火", "水", "木", "金", "土"};
    
    periods = {
        "14:40-15:50",
        "15:50-17:00",
        "17:00-18:10",
        "18:10-19:20",
        "19:20-20:30",
        "20:30-21:40"
    };

    allStudents = {
        {"小1", {{"生徒A", 1, 1, "", {"算数", "国語"}, ""}}},
        {"小2", {{"生徒B", 2, 2, "", {"算数", "国語"}, ""}}},
        {"小3", {{"生徒C", 3, 1, "", {"算数", "国語"}, ""}}},
        {"小4", {{"生徒D", 4, 2, "", {"算数", "国語"}, ""}}},
        {"小5", {{"生徒E", 5, 1, "", {"算数", "国語"}, ""}}},
        {"小6", {{"生徒F", 6, 2, "", {"算数", "国語"}, ""}}}
    };

    setupTable();
    setupEditor();
    ui->scheduleTable->setCurrentCell(0, 0);
    loadCell(0, 0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::lessonToJson(const CellData &lesson) const
{
	QJsonObject object;
	object["student1Name"] = lesson.student1Name;
	object["student1Grade"] = lesson.student1Grade;
	object["student1Subject"] = lesson.student1Subject;
	object["student2Name"] = lesson.student2Name;
	object["student2Grade"] = lesson.student2Grade;
	object["student2Subject"] = lesson.student2Subject;
	object["student1Memo"] = lesson.student1Memo;
	object["student2Memo"] = lesson.student2Memo;

	QJsonDocument document(object);

    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

QString MainWindow::lessonToJson(const int row, const int column) const
{
    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);

    if (row < 0 || row >= periods.size() || dayIndex < 0 || teacherIndex < 0)
    {
        return QString();
    }

    return lessonToJson(schedule[dayIndex][teacherIndex].lessons[row]);
}

CellData MainWindow::jsonToLesson(const QString &json) const
{
	const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

	if (!document.isObject())
	{
		statusBar()->showMessage("貼り付けできるセルデータではありません", 2000);
		return CellData();
	}

	const QJsonObject object = document.object();

	CellData lesson;
	lesson.student1Name = object.value("student1Name").toString();
	lesson.student1Grade = object.value("student1Grade").toString();
	lesson.student1Subject = object.value("student1Subject").toString();
	lesson.student2Name = object.value("student2Name").toString();
	lesson.student2Grade = object.value("student2Grade").toString();
	lesson.student2Subject = object.value("student2Subject").toString();
	lesson.student1Memo = object.value("student1Memo").toString();
	lesson.student2Memo = object.value("student2Memo").toString();

    return lesson;
}

QString MainWindow::scheduleToJson() const
{
    QJsonArray daysArray;

    for (const auto &day : schedule)
    {
        QJsonArray teachersArray;

        for (const auto &teacher : day)
        {
            QJsonObject teacherObject;
            teacherObject["teacherName"] = teacher.teacherName;

            QJsonArray lessonsArray;

            for (const auto &lesson : teacher.lessons)
            {
                lessonsArray.append(QJsonDocument::fromJson(lessonToJson(lesson).toUtf8()).object());
            }

            teacherObject["lessons"] = lessonsArray;
            teachersArray.append(teacherObject);
        }

        daysArray.append(teachersArray);
    }

    QJsonDocument document(daysArray);
    QString jsonString = QString::fromUtf8(document.toJson(QJsonDocument::Compact));
    return jsonString;
}

void MainWindow::jsonToSchedule(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isArray())
    {
        statusBar()->showMessage("読み込めるスケジュールデータではありません", 2000);
        return;
    }

    schedule.clear();

    const QJsonArray daysArray = document.array();

    for (int dayIndex = 0; dayIndex < daysArray.size() && dayIndex < schedule.size(); ++dayIndex)
    {
        const QJsonArray teachersArray = daysArray[dayIndex].toArray();

        for (int teacherIndex = 0; teacherIndex < teachersArray.size() && teacherIndex < schedule[dayIndex].size(); ++teacherIndex)
        {
            const QJsonObject teacherObject = teachersArray[teacherIndex].toObject();
            schedule[dayIndex][teacherIndex].teacherName = teacherObject.value("teacherName").toString();

            const QJsonArray lessonsArray = teacherObject.value("lessons").toArray();

            for (int lessonIndex = 0; lessonIndex < lessonsArray.size() && lessonIndex < schedule[dayIndex][teacherIndex].lessons.size(); ++lessonIndex)
            {
                const QJsonObject lessonObject = lessonsArray[lessonIndex].toObject();
                CellData lesson;
                lesson.student1Name = lessonObject.value("student1Name").toString();
                lesson.student1Grade = lessonObject.value("student1Grade").toString();
                lesson.student1Subject = lessonObject.value("student1Subject").toString();
                lesson.student2Name = lessonObject.value("student2Name").toString();
                lesson.student2Grade = lessonObject.value("student2Grade").toString();
                lesson.student2Subject = lessonObject.value("student2Subject").toString();
                lesson.student1Memo = lessonObject.value("student1Memo").toString();
                lesson.student2Memo = lessonObject.value("student2Memo").toString();

                schedule[dayIndex][teacherIndex].lessons[lessonIndex] = lesson;
            }
        }
    }
}

