#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::initializeTeacherLessons(TeacherColumn &teacher)
{
    teacher.lessons.clear();
    teacher.lessons.resize(periods.size());

    for (QVector<LessonData> &periodLessons : teacher.lessons)
    {
        periodLessons.resize(MaxStudentPerTeacher);
    }
}

void MainWindow::initializeTable()
{
    schedule.clear();

    for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex)
    {
        TeacherColumn emptyColumn;
        initializeTeacherLessons(emptyColumn);
        schedule.append(QVector<TeacherColumn>{emptyColumn});
    }

    clearCellEditHistory();
}

int MainWindow::tableRowCount() const
{
    return periods.size() * MaxStudentPerTeacher;
}

int MainWindow::periodIndexFromTableRow(int tableRow) const
{
    if (tableRow < 0 || MaxStudentPerTeacher <= 0)
    {
        return -1;
    }

    return tableRow / MaxStudentPerTeacher;
}

int MainWindow::studentIndexFromTableRow(int tableRow) const
{
    if (tableRow < 0 || MaxStudentPerTeacher <= 0)
    {
        return -1;
    }

    return tableRow % MaxStudentPerTeacher;
}

int MainWindow::tableRowOf(int periodIndex, int studentIndex) const
{
    if (periodIndex < 0 || periodIndex >= periods.size() ||
        studentIndex < 0 || studentIndex >= MaxStudentPerTeacher)
    {
        return -1;
    }

    return periodIndex * MaxStudentPerTeacher + studentIndex;
}

int MainWindow::firstColumnOfDay(int dayIndex) const
{
    int column = 0;

    for (int i = 0; i < dayIndex && i < schedule.size(); ++i)
    {
        column += schedule[i].size();
    }

    return column;
}

int MainWindow::columnCountOfDay(int dayIndex) const
{
    if (dayIndex < 0 || dayIndex >= schedule.size())
    {
        return 0;
    }

    return schedule[dayIndex].size();
}

int MainWindow::dayIndexFromColumn(int column) const
{
    int firstColumn = 0;

    for (int dayIndex = 0; dayIndex < schedule.size(); ++dayIndex)
    {
        const int count = schedule[dayIndex].size();

        if (column >= firstColumn && column < firstColumn + count)
        {
            return dayIndex;
        }

        firstColumn += count;
    }

    return -1;
}

int MainWindow::teacherIndexFromColumn(int column) const
{
    const int dayIndex = dayIndexFromColumn(column);

    if (dayIndex < 0)
    {
        return -1;
    }

    return column - firstColumnOfDay(dayIndex);
}

QString MainWindow::cellTextFromData(const LessonData &lesson) const
{
    QStringList lines;

    const QString info = QString("%1 %2")
                             .arg(lesson.studentGrade.trimmed(), lesson.subject.trimmed())
                             .trimmed();

    if (!info.isEmpty())
    {
        lines << info;
    }

    if (!lesson.studentName.trimmed().isEmpty())
    {
        lines << lesson.studentName.trimmed();
    }

    return lines.join('\n');
}

bool MainWindow::lessonDataIsEmpty(const LessonData &lesson) const
{
    return lesson.studentName.trimmed().isEmpty() &&
           lesson.studentGrade.trimmed().isEmpty() &&
           lesson.subject.trimmed().isEmpty() &&
           lesson.memo.trimmed().isEmpty();
}

QString MainWindow::lessonToJson(const LessonData &lesson) const
{
    QJsonObject object;
    object["studentName"] = lesson.studentName;
    object["studentGrade"] = lesson.studentGrade;
    object["subject"] = lesson.subject;
    object["memo"] = lesson.memo;

    return QString::fromUtf8(
        QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString MainWindow::lessonToJson(int row, int column) const
{
    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);
    const int periodIndex = periodIndexFromTableRow(row);
    const int studentIndex = studentIndexFromTableRow(row);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || studentIndex < 0)
    {
        return QString();
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return QString();
    }

    return lessonToJson(
        schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex]);
}

LessonData MainWindow::jsonToLesson(const QString &json) const
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject())
    {
        statusBar()->showMessage("貼り付けできるセルデータではありません", 2000);
        return LessonData();
    }

    const QJsonObject object = document.object();
    LessonData lesson;

    if (object.contains("studentName"))
    {
        lesson.studentName = object.value("studentName").toString();
        lesson.studentGrade = object.value("studentGrade").toString();
        lesson.subject = object.value("subject").toString();
        lesson.memo = object.value("memo").toString();
        return lesson;
    }

    // 旧形式のコピー内容は、生徒1として貼り付ける。
    lesson.studentName = object.value("student1Name").toString();
    lesson.studentGrade = object.value("student1Grade").toString();
    lesson.subject = object.value("student1Subject").toString();
    lesson.memo = object.value("student1Memo").toString();

    return lesson;
}
