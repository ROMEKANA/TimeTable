#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDate>
#include <QMainWindow>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class QComboBox;
class QColor;
class QEvent;
class QJsonObject;
class QPainter;
class QPrinter;
class QRectF;
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

struct StudentSubjectData
{
    QString subjectName;
    QStringList materials;
};

struct StudentData
{
    QString Name;
    int Grade;
    int gender;
    QString memo;
    QVector<StudentSubjectData> subjects;
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
    int oneOnTwoRate;
    int oneOnOneRate;
    int transportPay;
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
    int cellSectionSize = 115;
    QString scheduleOddRowColor = "#f4f4f4";
    QString scheduleTextColor = "#000000";
    QString scheduleOddRowTextColor = "#000000";
    QString scheduleVerticalLineColor = "#7d7d7d";
    int scheduleVerticalLineWidth = 1;
    QString scheduleHorizontalLineColor = "#7d7d7d";
    int scheduleHorizontalLineWidth = 1;
    QString scheduleVerticalSectionLineColor = "#373737";
    int scheduleVerticalSectionLineWidth = 2;
    QString scheduleHorizontalSectionLineColor = "#373737";
    int scheduleHorizontalSectionLineWidth = 2;
    int schedulePrintDarknessPercent = 115;
    int schedulePrintLineWidthPercent = 100;
    int schedulePrintSizePercent = 96;
    int defaultSalaryOneOnTwoRate = 2000;
    int defaultSalaryOneOnOneRate = 1000;
    int defaultSalaryTransportPay = 0;
    int defaultSalaryBusinessPay = 0;
    int defaultSalaryHighSchoolAllowance = 500;

    // General
    QString dataFilePath(QString data);
    void loadMasterData();
    void showMasterDataDialog();
    QJsonObject loadMasterJson();
    QStringList masterListDefaultValues(const QString &key) const;
    void normalizeMasterJson(QJsonObject *root) const;
    bool saveMasterJson(const QJsonObject &root);
    void refreshAfterMasterDataChanged();
    void addMasterListValue(const QString &key, const QString &label);
    void deleteMasterListValue(const QString &key, const QString &label);
    void setupActions();

    // schedule Tab
    int selectedRow = -1;
    int selectedColumn = -1;
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
    void updateSubjectComboBoxForStudent(
        QComboBox *comboBox,
        const QString &grade,
        const QString &studentName);
    void updateTeacherComboBox(QComboBox *comboBox);

    void saveScheduleToFile();
    void loadScheduleButton();

    void showLastWeek();
    void showThisWeek();
    void showNextWeek();
    void copyCurrentWeekToThisWeek();
    void copySelectedWeekToCurrentWeek();

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
    QStringList subjectNamesForStudent(
        const QString &grade,
        const QString &studentName) const;
    QStringList materialNamesForStudentSubject(
        const QString &grade,
        const QString &studentName,
        const QString &subjectName) const;

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
    void showTeacherDailyPrintPreview();
    void showSalaryStatementPrintPreview();
    void showGuidanceReportPrintPreview();
    void copyStudentScheduleToClipboard();
    void renderScheduleForPrint(QPrinter *printer);
    void renderTeacherDailyReportForPrint(
        QPrinter *printer,
        const QString &teacherName,
        const QDate &date);
    void renderSalaryStatementForPrint(
        QPrinter *printer,
        const QString &teacherName,
        const QDate &month,
        const QVector<int> &deductions);
    void renderGuidanceReportFormatForPrint(
        QPrinter *printer,
        const QString &grade,
        const QString &studentName,
        const QString &subjectName,
        const QString &materialName);
    bool findStudentData(
        const QString &grade,
        const QString &studentName,
        StudentData *student) const;
    bool findTeacherData(
        const QString &teacherName,
        TeacherData *teacher) const;
    bool selectStudentSubject(
        QString *grade,
        QString *studentName,
        QString *subjectName,
        QString *materialName,
        const QString &title,
        bool requireSubject,
        bool includeMaterial);
    bool findNextLessonForStudent(
        const LessonRecord &baseLesson,
        LessonRecord *nextLesson) const;
    QString studentScheduleText(
        const QString &grade,
        const QString &studentName,
        const QString &subjectName) const;
    int totalScheduleTeacherColumns() const;
    QRectF schedulePrintContentRect(QPrinter *printer) const;
    qreal schedulePrintLineWidth(QPainter *painter, int width) const;
    QColor schedulePrintColor(const QString &colorName) const;
    void drawSchedulePrintText(
        QPainter *painter,
        const QRectF &rect,
        const QString &text,
        int alignment,
        bool bold = false) const;
    void fillSchedulePrintRowBackground(
        QPainter *painter,
        const QRectF &rect,
        int tableRow) const;
    void drawSchedulePrintLines(
        QPainter *painter,
        const QRectF &rect,
        bool drawRightSection,
        bool drawBottomSection) const;
    void drawSchedulePrintHeader(
        QPainter *painter,
        const QRectF &area,
        qreal timeColumnWidth,
        qreal teacherColumnWidth,
        qreal dayHeaderHeight,
        qreal teacherHeaderHeight) const;
    void drawSchedulePrintBody(
        QPainter *painter,
        const QRectF &area,
        qreal timeColumnWidth,
        qreal teacherColumnWidth,
        qreal headerHeight,
        qreal studentRowHeight) const;
};

#endif // MAINWINDOW_H
