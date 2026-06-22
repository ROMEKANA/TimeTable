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
    QStringList teachers;

    QVector<QVector<TeacherColumn>> schedule;

    // General
    QString dataFilePath(QString data);
    void loadMasterData();

    QString lessonToJson(const CellData &lesson) const;
    QString lessonToJson(const int row, const int column) const;
    CellData jsonToLesson(const QString &json) const;
    QString scheduleToJson() const;
    void jsonToSchedule(const QString &json);

    void saveScheduleToFile();
    void loadScheduleFromFile();

    void loadLatestSchedule();
    QString schedulesDirPath() const;

    // scheduleTab
    int selectedRow = -1;
    int selectedColumn = -1;
    int cellDefaultSectionSize = 145;

    void setupTable();
    void setupEditor();

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


};

#endif // MAINWINDOW_H
