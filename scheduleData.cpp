#include "mainwindow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStatusBar>

// 講師列の授業枠を現在の時限数と最大生徒数で初期化する
void MainWindow::initializeTeacherLessons(TeacherColumn &teacher)
{
    teacher.lessons.clear();
    teacher.lessons.resize(periods.size());

    for (QVector<LessonData> &periodLessons : teacher.lessons)
    {
        periodLessons.resize(MaxStudentPerTeacher);
    }

    normalizeTeacherLessonMaxStudents(teacher);
}

// 講師列に保存された最大生徒数を有効範囲へ補正する
void MainWindow::normalizeTeacherLessonMaxStudents(TeacherColumn &teacher)
{
    for (QVector<LessonData> &periodLessons : teacher.lessons)
    {
        for (LessonData &lesson : periodLessons)
        {
            if (lessonDataIsEmpty(lesson))
            {
                lesson.maxStudents = 0;
                continue;
            }

            if (lesson.maxStudents < 0)
            {
                lesson.maxStudents = 0;
            }

            if (lesson.maxStudents > 0)
            {
                lesson.maxStudents =
                    qBound(1, lesson.maxStudents, MaxStudentPerTeacher);
            }
        }
    }
}

// 曜日ごとに空の講師列を持つ時間割を初期化する
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

// 時間割テーブルに必要な総行数を返す
int MainWindow::tableRowCount() const
{
    return periods.size() * MaxStudentPerTeacher;
}

// テーブル行から時限インデックスを求める
int MainWindow::periodIndexFromTableRow(int tableRow) const
{
    if (tableRow < 0 || MaxStudentPerTeacher <= 0)
    {
        return -1;
    }

    return tableRow / MaxStudentPerTeacher;
}

// テーブル行からコマ内の生徒インデックスを求める
int MainWindow::studentIndexFromTableRow(int tableRow) const
{
    if (tableRow < 0 || MaxStudentPerTeacher <= 0)
    {
        return -1;
    }

    return tableRow % MaxStudentPerTeacher;
}

// 時限と生徒のインデックスからテーブル行を求める
int MainWindow::tableRowOf(int periodIndex, int studentIndex) const
{
    if (periodIndex < 0 || periodIndex >= periods.size() ||
        studentIndex < 0 || studentIndex >= MaxStudentPerTeacher)
    {
        return -1;
    }

    return periodIndex * MaxStudentPerTeacher + studentIndex;
}

// 指定した授業枠に設定された最大生徒数を返す
int MainWindow::lessonMaxStudentsAt(
    int dayIndex,
    int teacherIndex,
    int periodIndex) const
{
    if (dayIndex < 0 || dayIndex >= schedule.size() ||
        teacherIndex < 0 || teacherIndex >= schedule[dayIndex].size() ||
        periodIndex < 0 || periodIndex >= periods.size())
    {
        return MaxStudentPerTeacher;
    }

    const TeacherColumn &teacher = schedule[dayIndex][teacherIndex];

    if (periodIndex >= teacher.lessons.size())
    {
        return MaxStudentPerTeacher;
    }

    int maxStudents = MaxStudentPerTeacher;

    for (const LessonData &lesson : teacher.lessons[periodIndex])
    {
        if (!lessonDataIsEmpty(lesson) && lesson.maxStudents > 0)
        {
            maxStudents = qMin(
                maxStudents,
                qBound(1, lesson.maxStudents, MaxStudentPerTeacher));
        }
    }

    return maxStudents;
}

// 指定した曜日の先頭列を返す
int MainWindow::firstColumnOfDay(int dayIndex) const
{
    int column = 0;

    for (int i = 0; i < dayIndex && i < schedule.size(); ++i)
    {
        column += schedule[i].size();
    }

    return column;
}

// 指定した曜日の講師列数を返す
int MainWindow::columnCountOfDay(int dayIndex) const
{
    if (dayIndex < 0 || dayIndex >= schedule.size())
    {
        return 0;
    }

    return schedule[dayIndex].size();
}

// テーブル列から曜日インデックスを求める
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

// テーブル列から曜日内の講師インデックスを求める
int MainWindow::teacherIndexFromColumn(int column) const
{
    const int dayIndex = dayIndexFromColumn(column);

    if (dayIndex < 0)
    {
        return -1;
    }

    return column - firstColumnOfDay(dayIndex);
}

// 授業データからセル表示用の文字列を作る
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

// 授業データに入力内容がないか確認する
bool MainWindow::lessonDataIsEmpty(const LessonData &lesson) const
{
    return lesson.studentName.trimmed().isEmpty() &&
           lesson.studentGrade.trimmed().isEmpty() &&
           lesson.subject.trimmed().isEmpty() &&
           lesson.memo.trimmed().isEmpty();
}

// 現在の時間割を授業記録の一覧に変換する
QVector<LessonRecord> MainWindow::scheduleEntries() const
{
    return scheduleEntriesFor(scheduleMonday, schedule);
}

// 指定した時間割を現在の曜日・時限で授業記録の一覧に変換する
QVector<LessonRecord> MainWindow::scheduleEntriesFor(
    const QDate &monday,
    const QVector<QVector<TeacherColumn>> &scheduleData) const
{
    return scheduleEntriesFor(monday, scheduleData, days, periods);
}

// 指定した時間割と見出しから授業記録の一覧を作る
QVector<LessonRecord> MainWindow::scheduleEntriesFor(
    const QDate &monday,
    const QVector<QVector<TeacherColumn>> &scheduleData,
    const QStringList &scheduleDays,
    const QStringList &schedulePeriods) const
{
    QVector<LessonRecord> entries;

    for (int dayIndex = 0; dayIndex < scheduleData.size(); ++dayIndex)
    {
        const QVector<TeacherColumn> &daySchedule = scheduleData[dayIndex];
        const QDate date = monday.addDays(dayIndex);
        const QString day = scheduleDays.value(dayIndex);

        for (int teacherIndex = 0; teacherIndex < daySchedule.size(); ++teacherIndex)
        {
            const TeacherColumn &teacher = daySchedule[teacherIndex];

            for (int periodIndex = 0;
                 periodIndex < teacher.lessons.size() &&
                 periodIndex < schedulePeriods.size();
                 ++periodIndex)
            {
                const QVector<LessonData> &periodLessons = teacher.lessons[periodIndex];

                for (int studentIndex = 0; studentIndex < periodLessons.size(); ++studentIndex)
                {
                    const LessonData &lesson = periodLessons[studentIndex];

                    if (lessonDataIsEmpty(lesson))
                    {
                        continue;
                    }

                    LessonRecord entry;
                    entry.date = date;
                    entry.day = day;
                    entry.period = schedulePeriods.value(periodIndex);
                    entry.teacherName = teacher.teacherName;
                    entry.studentName = lesson.studentName;
                    entry.studentGrade = lesson.studentGrade;
                    entry.subject = lesson.subject;
                    entry.memo = lesson.memo;
                    entry.dayIndex = dayIndex;
                    entry.teacherIndex = teacherIndex;
                    entry.periodIndex = periodIndex;
                    entry.studentIndex = studentIndex;

                    entries.append(entry);
                }
            }
        }
    }

    return entries;
}

// 授業データをクリップボード用JSONへ変換する
QString MainWindow::lessonToJson(const LessonData &lesson) const
{
    QJsonObject object;
    object["studentName"] = lesson.studentName;
    object["studentGrade"] = lesson.studentGrade;
    object["subject"] = lesson.subject;
    object["memo"] = lesson.memo;
    object["maxStudents"] = lesson.maxStudents;

    return QString::fromUtf8(
        QJsonDocument(object).toJson(QJsonDocument::Compact));
}

// 指定セルの授業データをクリップボード用JSONへ変換する
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

// クリップボードのJSONを授業データへ変換する
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
        lesson.maxStudents = qMax(0, object.value("maxStudents").toInt());
        return lesson;
    }

    // 旧形式のコピー内容は、生徒1として貼り付ける。
    lesson.studentName = object.value("student1Name").toString();
    lesson.studentGrade = object.value("student1Grade").toString();
    lesson.subject = object.value("student1Subject").toString();
    lesson.memo = object.value("student1Memo").toString();
    lesson.maxStudents = qMax(0, object.value("maxStudents").toInt());

    return lesson;
}
