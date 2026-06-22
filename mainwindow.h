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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupTable();
    void setupEditor();

    QStringList days = {"月", "火", "水", "木", "金", "土"};
    QStringList periods = {
        "14:40-15:50",
        "15:50-17:00",
        "17:00-18:10",
        "18:10-19:20",
        "19:20-20:30",
        "20:30-21:40"
    };

    QVector<QVector<TeacherColumn>> schedule;

    void initializeSchedule();
    void rebuildScheduleTable();

    int dayIndexFromColumn(int column) const;
    int teacherIndexFromColumn(int column) const;
    int firstColumnOfDay(int dayIndex) const;
    int columnCountOfDay(int dayIndex) const;

    void addTeacherColumn();
    void removeTeacherColumn();
    void renameTeacherColumn();

    void loadCell(int row, int column);
    void updateCell();
    void clearSelectedCell();

    // void saveToFile();
    // void loadFromFile();

    void clearEntry(int row, int column);
    void renderEntry(int row, int column);
    
    bool entryIsEmpty(const CellData &cell) const;
    QString cellTextFromData(const CellData &cell) const;

    Ui::MainWindow *ui;

    int selectedRow = -1;
    int selectedColumn = -1;
};

#endif // MAINWINDOW_H
