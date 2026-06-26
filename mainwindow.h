#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAbstractItemView>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QInputDialog>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonParseError>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QFontMetrics>
#include <QDate>
#include <algorithm>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

struct CellData
{
    QString student1Name;
    QString student1Grade;
    QString student1Subject;

    QString student2Name;
    QString student2Grade;
    QString student2Subject;

    QString student1Memo;
    QString student2Memo;
};

struct TeacherColumn
{
    QString teacherName;
    QVector<CellData> lessons;
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
    // QString id;
    QString name;
    QString memo;
    // QStringList subjects;
    // QStringList availableDays;
    // bool enabled = true;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    // membar variables
    Ui::MainWindow *ui;

    QStringList days;
    QStringList periods;

    QStringList grades;
    QStringList genders;
    QStringList subjects;

    QVector<GradeStudents> allStudents;
    QVector<TeacherData> teachers;

    QVector<QVector<TeacherColumn>> schedule;

    int MaxStudentPerTeacher = 2;

    // General
    QString dataFilePath(QString data);
    void loadMasterData();

    // scheduleTab
    int selectedRow = -1;
    int selectedColumn = -1;
    int cellDefaultSectionSize = 115;

    QDate scheduleMonday;

    void setupTable();
    void scheduleTabConnects();

    void initializeTable();
    void renderTable();

    int firstColumnOfDay(int dayIndex) const;
    int columnCountOfDay(int dayIndex) const;
    int dayIndexFromColumn(int column) const;
    int teacherIndexFromColumn(int column) const;

    void addTeacherColumn();
    void removeTeacherColumn();
    void renameTeacherColumn();

    void loadCell(int row, int column);
    void updateCell();
    QString cellTextFromData(const CellData &cell) const;
    void renderCell(int row, int column);
    void clearCell();

    void renderEntry();

    void copyCell();
    void pasteCell();

    bool celldataIsEmpty(const CellData &cell) const;

    void updateStudentComboBox(QComboBox *comboBox, const QString &grade);
    void updateTeacherComboBox(QComboBox *comboBox);

    QString lessonToJson(const CellData &lesson) const;
    QString lessonToJson(const int row, const int column) const;
    CellData jsonToLesson(const QString &json) const;

    QString scheduleToJson() const;
    bool jsonToSchedule(const QString &json);

    void saveScheduleToFile();
    void loadScheduleButton();
    void loadLatestSchedule();

    bool loadScheduleFromFile(const QDate &monday);

    void switchScheduleWeek(const QDate &date);

    void showLastWeek();
    void showThisWeek();
    void showNextWeek();

    QDate mondayOf(const QDate &date) const;

    QString schedulesDirPath() const;
    QString scheduleFilePath(const QDate &monday);

    // studentTab
    void setupStudentTab();
    void renderStudentList();
    void loadStudent(int index);

    void renderStudentEntry();
    void clearStudentEntry();

    void addStudent();
    void removeStudent();
    void saveStudent();
    void loadStudent();

    bool saveStudentsToFile(const QVector<GradeStudents> &allStudents);

    // teacherTab
    void setupTeacherTab();
    void renderTeacherList();
    void loadTeacher(int index);

    void renderTeacherEntry();
    void clearTeacherEntry();

    void addTeacher();
    void removeTeacher();
    void saveTeacher();
    void loadTeacher();

    bool saveTeachersToFile();

    // exportTab
    void setupExportTab();

    void showSchedulePrintPreview();
    void renderScheduleForPrint(QPrinter *printer);
};

#endif // MAINWINDOW_H
