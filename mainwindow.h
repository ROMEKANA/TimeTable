#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QVector>

class QTableWidgetItem;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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
        "20:30-21:40"};

    QVector<QStringList> teacherNamesByDay;

    void initializeTeacherColumns();
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

    //void saveToFile();
    //void loadFromFile();

    //void resetForm();

    void clearEntry(int row, int column);
    void renderEntry(int row, int column);
    bool entryIsEmpty(const QTableWidgetItem *item) const;
    QString cellTextFromItem(const QTableWidgetItem *item) const;

    Ui::MainWindow *ui;

    int selectedRow = -1;
    int selectedColumn = -1;
};

#endif // MAINWINDOW_H
