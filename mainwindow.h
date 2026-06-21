#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidgetItem;
class QTextEdit;

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
    void loadCell(int row, int column);
    void updateCell();
    void clearSelectedCell();
    void saveToFile();
    void loadFromFile();
    void resetForm();
    void clearEntry(int row, int column);
    void renderEntry(int row, int column);
    bool entryIsEmpty(const QTableWidgetItem *item) const;
    QString cellTextFromItem(const QTableWidgetItem *item) const;

    Ui::MainWindow *ui;
    QLineEdit *teacherEdit = nullptr;
    QLineEdit *roomEdit = nullptr;
    QLineEdit *student1Edit = nullptr;
    QComboBox *grade1Box = nullptr;
    QComboBox *subject1Box = nullptr;
    QLineEdit *student2Edit = nullptr;
    QComboBox *grade2Box = nullptr;
    QComboBox *subject2Box = nullptr;
    QTextEdit *extraStudentsEdit = nullptr;
    QLineEdit *memoEdit = nullptr;
    QPushButton *applyButton = nullptr;
    QPushButton *clearButton = nullptr;
    int selectedRow = -1;
    int selectedColumn = -1;
};

#endif // MAINWINDOW_H
