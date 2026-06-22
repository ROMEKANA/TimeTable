#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QVector>

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

struct oneGradeStudents
{
    QString Grade;
    QStringList students;
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
    QStringList subjects;
    QStringList teachers;

    QVector<QVector<TeacherColumn>> schedule;

    QVector<oneGradeStudents> students;

    // General
    QString lessonToJson(const CellData &lesson) const;
    QString lessonToJson(const int row, const int column) const;
    CellData jsonToLesson(const QString &json) const;
    QString scheduleToJson() const;
    void jsonToSchedule(const QString &json);

    void saveScheduleToFile();
    void loadScheduleFromFile();

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
    void renderCell(int row, int column);
    void clearCell();

    void renderEntry();

    void copyCell();
    void pasteCell();
    
    bool entryIsEmpty(const CellData &cell) const;
    QString cellTextFromData(const CellData &cell) const;
};

#endif // MAINWINDOW_H
