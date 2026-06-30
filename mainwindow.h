#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDate>
#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QEvent;
class QObject;
class QPrinter;
class QWidget;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

struct LessonData
{
    QString studentName;
    QString studentGrade;
    QString subject;
    QString memo;
};

struct LessonRecord
{
    QDate date;
    QString day;
    QString period;
    QString teacherName;
    QString studentName;
    QString studentGrade;
    QString subject;
    QString memo;
    int dayIndex;
    int teacherIndex;
    int periodIndex;
    int studentIndex;
};

struct CellEditCommand
{
    int row;
    int column;
    LessonData before;
    LessonData after;
};

struct TeacherColumn
{
    QString teacherName;
    QVector<QVector<LessonData>> lessons;
};

struct StudentData
{
    QString Name;
    int Grade;
    int gender;
    QString memo;
    QStringList subjects;
    QString school;
};

struct GradeStudents
{
    QString Grade;
    QVector<StudentData> students;
};

struct TeacherData
{
    QString name;
    QString memo;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;

    QStringList days;
    QStringList periods;

    QStringList grades;
    QStringList genders;
    QStringList subjects;

    QStringList schools;

    QVector<GradeStudents> allStudents;
    QVector<TeacherData> teachers;

    QVector<QVector<TeacherColumn>> schedule;

    QVector<CellEditCommand> undoStack;
    QVector<CellEditCommand> redoStack;

    int MaxStudentPerTeacher = 2;
    float scrollSpeed = 0.01f;

    // General
    QString dataFilePath(QString data);
    void loadMasterData();
    void setupActions();

    // schedule Tab
    int selectedRow = -1;
    int selectedColumn = -1;
    int cellSectionSize = 115;
    bool isLoadingCell = false;

    QDate scheduleMonday;

    void setupScheduleTab();
    void scheduleTabConnects();

    void renderTable();

    void addTeacherColumn();
    void removeTeacherColumn();
    void renameTeacherColumn();

    bool isValidCellIndex(int row, int column); //schedule tabで使う、表示用のインデックスチェック

    void loadCell(int row, int column);
    void updateCell();
    void renderCell(int row, int column);
    void clearCell();
    void renderEntry();

    void copyCell();
    void pasteCell();
    void cutCell();

    void updateStudentComboBox(QComboBox *comboBox, const QString &grade);
    void updateTeacherComboBox(QComboBox *comboBox);

    void saveScheduleToFile();
    void loadScheduleButton();

    void showLastWeek();
    void showThisWeek();
    void showNextWeek();
    void copyCurrentWeekToThisWeek();

    // schedule data
    void initializeTeacherLessons(TeacherColumn &teacher);
    void initializeTable();

    int tableRowCount() const;
    int periodIndexFromTableRow(int tableRow) const;
    int studentIndexFromTableRow(int tableRow) const;
    int tableRowOf(int periodIndex, int studentIndex) const;

    int firstColumnOfDay(int dayIndex) const;
    int columnCountOfDay(int dayIndex) const;
    int dayIndexFromColumn(int column) const;
    int teacherIndexFromColumn(int column) const;

    QString cellTextFromData(const LessonData &lesson) const;
    bool lessonDataIsEmpty(const LessonData &lesson) const;
    QVector<LessonRecord> scheduleEntries() const;
    QVector<LessonRecord> scheduleEntriesFor(
        const QDate &monday,
        const QVector<QVector<TeacherColumn>> &scheduleData) const;

    QString lessonToJson(const LessonData &lesson) const;
    QString lessonToJson(int row, int column) const;
    LessonData jsonToLesson(const QString &json) const;

    // schedule storage
    bool jsonToScheduleData(
        const QString &json,
        QDate *monday,
        QVector<QVector<TeacherColumn>> *loadedSchedule) const;
    bool loadScheduleDataFromFile(
        const QDate &monday,
        QDate *fileMonday,
        QVector<QVector<TeacherColumn>> *loadedSchedule) const;

    QString scheduleToJson() const;
    bool jsonToSchedule(const QString &json);

    void loadLatestSchedule();
    bool loadScheduleFromFile(const QDate &monday);

    void switchScheduleWeek(const QDate &date);

    QDate mondayOf(const QDate &date) const;
    QString schedulesDirPath() const;
    QString scheduleFilePath(const QDate &monday);

    // undo
    void undoCellEdit();
    void redoCellEdit();
    bool lessonDataEquals(
        const LessonData &a,
        const LessonData &b) const;

    bool setLessonAtCell(
        int row,
        int column,
        const LessonData &lesson);

    void pushCellEdit(
        int row,
        int column,
        const LessonData &before,
        const LessonData &after);

    void clearCellEditHistory();
    void updateUndoRedoButtons();

    // student Tab
    void setupStudentTab();
    void renderStudentList();
    void loadStudent(int index);
    void renderStudentEntry();
    void clearStudentEntry();
    void removeStudent();
    void saveStudent();
    void loadStudent();
    bool saveStudentsToFile(const QVector<GradeStudents> &allStudents);

    void updateSchoolComboBox();
    void addSchoolList();
    void deleteSchoolList();
    void saveSchoolList();
    void loadSchoolList();

    // teacher Tab
    void setupTeacherTab();
    void renderTeacherList();
    void loadTeacher(int index);
    void renderTeacherEntry();
    void clearTeacherEntry();
    void removeTeacher();
    void saveTeacher();
    void loadTeacher();
    bool saveTeachersToFile();

    // export Tab
    void setupExportTab();
    void showSchedulePrintPreview();
    void renderScheduleForPrint(QPrinter *printer);
};

#endif // MAINWINDOW_H
