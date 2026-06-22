#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("塾時間割表");
    // resize(1500, 760);

    days = {"月", "火", "水", "木", "金", "土"};

    periods = {
        "14:40-15:50",
        "15:50-17:00",
        "17:00-18:10",
        "18:10-19:20",
        "19:20-20:30",
        "20:30-21:40"};

    allStudents = {
        {"小1", {{"生徒A", 1, 1, "", {"算数", "国語"}, ""}}},
        {"小2", {{"生徒B", 2, 2, "", {"算数", "国語"}, ""}}},
        {"小3", {{"生徒C", 3, 1, "", {"算数", "国語"}, ""}}},
        {"小4", {{"生徒D", 4, 2, "", {"算数", "国語"}, ""}}},
        {"小5", {{"生徒E", 5, 1, "", {"算数", "国語"}, ""}}},
        {"小6", {{"生徒F", 6, 2, "", {"算数", "国語"}, ""}}}};

        grades = {"小1", "小2", "小3", "小4", "小5", "小6", "中1", "中2", "中3", "高1", "高2", "高3", "既卒"};

        genders = {"男性", "女性", "その他"};

        subjects = {"英語", "数学", "国語", "理科", "社会", "算数", "理社", "その他"};

    setupTable();
    loadLatestSchedule();
    setupEditor();

    setupStudentTab();

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

    for (int dayIndex = 0; dayIndex < daysArray.size(); ++dayIndex)
    {
        QVector<TeacherColumn> dayColumns;
        const QJsonArray teachersArray = daysArray[dayIndex].toArray();

        for (const QJsonValue &teacherValue : teachersArray)
        {
            const QJsonObject teacherObject = teacherValue.toObject();

            TeacherColumn teacher;
            teacher.teacherName = teacherObject.value("teacherName").toString();
            teacher.lessons.resize(periods.size());

            const QJsonArray lessonsArray = teacherObject.value("lessons").toArray();

            for (int lessonIndex = 0; lessonIndex < lessonsArray.size() && lessonIndex < periods.size(); ++lessonIndex)
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

                teacher.lessons[lessonIndex] = lesson;
            }

            dayColumns.append(teacher);
        }

        if (dayColumns.isEmpty())
        {
            TeacherColumn emptyColumn;
            emptyColumn.lessons.resize(periods.size());
            dayColumns.append(emptyColumn);
        }

        schedule.append(dayColumns);
    }

    while (schedule.size() < days.size())
    {
        TeacherColumn emptyColumn;
        emptyColumn.lessons.resize(periods.size());
        schedule.append(QVector<TeacherColumn>{emptyColumn});
    }

    renderTable();
}

QString MainWindow::schedulesDirPath() const
{
    return QCoreApplication::applicationDirPath() + "/schedules";
}

void MainWindow::saveScheduleToFile()
{
    QDir dir(schedulesDirPath());

    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    const QString json = scheduleToJson();

    const QString datedFileName =
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss") + ".json";

    const QString datedPath = dir.filePath(datedFileName);
    const QString latestPath = dir.filePath("latest.json");

    QFile datedFile(datedPath);
    if (!datedFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "保存エラー", "時間割を保存できませんでした。");
        return;
    }

    datedFile.write(json.toUtf8());
    datedFile.close();

    QFile latestFile(latestPath);
    if (latestFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        latestFile.write(json.toUtf8());
        latestFile.close();
    }

    statusBar()->showMessage("時間割を保存しました", 2000);
}

void MainWindow::loadScheduleFromFile()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "時間割を読み込み",
        schedulesDirPath(),
        "JSON (*.json)");

    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "読み込みエラー", "時間割を読み込めませんでした。");
        return;
    }

    jsonToSchedule(QString::fromUtf8(file.readAll()));
    statusBar()->showMessage("時間割を読み込みました", 2000);
}

void MainWindow::loadLatestSchedule()
{
    const QString latestPath = QDir(schedulesDirPath()).filePath("latest.json");

    QFile file(latestPath);

    if (!file.exists())
    {
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    jsonToSchedule(QString::fromUtf8(file.readAll()));
    statusBar()->showMessage("最新の時間割を読み込みました", 2000);
}