#include "mainwindow.h"
#include "ui_mainwindow.h"

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

namespace
{
    enum DataRole
    {
        TeacherRole = Qt::UserRole,
        Student1NameRole,
        Student1GradeRole,
        Student1SubjectRole,
        Student2NameRole,
        Student2GradeRole,
        Student2SubjectRole,
        Student1MemoRole,
        Student2MemoRole
    };

    QStringList grades()
    {
        return {"", "小1", "小2", "小3", "小4", "小5", "小6", "中1", "中2", "中3", "高1", "高2", "高3", "既卒"};
    }

    QStringList subjects()
    {
        return {"", "英語", "数学", "国語", "理科", "社会", "算数", "理社", "その他"};
    }

    QStringList teachers()
    {
        return {"", "講師1", "講師2", "講師3", "講師4"};
    }

    QStringList students()
    {
        return {"", "生徒1", "生徒2", "生徒3", "生徒4", "生徒5"};
    }

    // Creates a formatted string for a student's information
    QString studentLine(const QString &name, const QString &grade, const QString &subject)
    {
        QStringList parts;
        if (!name.trimmed().isEmpty())
        {
            parts << name.trimmed();
        }
        if (!grade.trimmed().isEmpty())
        {
            parts << grade.trimmed();
        }
        if (!subject.trimmed().isEmpty())
        {
            parts << subject.trimmed();
        }
        return grade + " " + subject + " " + name;
    }
}

void MainWindow::setupTable()
{
    initializeTeacherColumns();
    rebuildScheduleTable();
}

void MainWindow::initializeTeacherColumns()
{
    teacherNamesByDay.clear();

    for (int i = 0; i < days.size(); ++i)
    {
        teacherNamesByDay.append({""});
    }
}

void MainWindow::rebuildScheduleTable()
{
    QStringList headers;

    for (int dayIndex = 0; dayIndex < teacherNamesByDay.size(); ++dayIndex)
    {
        for (int teacherIndex = 0; teacherIndex < teacherNamesByDay[dayIndex].size(); ++teacherIndex)
        {
            const QString teacherName = teacherNamesByDay[dayIndex][teacherIndex].trimmed();

            if (teacherName.isEmpty())
            {
                headers << QString("%1\n講師未設定").arg(days[dayIndex]);
            }
            else
            {
                headers << QString("%1\n%2").arg(days[dayIndex], teacherName);
            }
        }
    }

    ui->scheduleTable->clear();
    ui->scheduleTable->setColumnCount(headers.size());
    ui->scheduleTable->setRowCount(periods.size());
    ui->scheduleTable->setHorizontalHeaderLabels(headers);
    ui->scheduleTable->setVerticalHeaderLabels(periods);

    ui->scheduleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->scheduleTable->horizontalHeader()->setDefaultSectionSize(145);
    ui->scheduleTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->scheduleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->scheduleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->scheduleTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->scheduleTable->setWordWrap(true);

    for (int row = 0; row < ui->scheduleTable->rowCount(); ++row)
    {
        for (int column = 0; column < ui->scheduleTable->columnCount(); ++column)
        {
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
            ui->scheduleTable->setItem(row, column, item);
        }
    }
}

int MainWindow::firstColumnOfDay(int dayIndex) const
{
    int column = 0;

    for (int i = 0; i < dayIndex; ++i)
    {
        column += teacherNamesByDay[i].size();
    }

    return column;
}

int MainWindow::columnCountOfDay(int dayIndex) const
{
    if (dayIndex < 0 || dayIndex >= teacherNamesByDay.size())
    {
        return 0;
    }

    return teacherNamesByDay[dayIndex].size();
}

int MainWindow::dayIndexFromColumn(int column) const
{
    int firstColumn = 0;

    for (int dayIndex = 0; dayIndex < teacherNamesByDay.size(); ++dayIndex)
    {
        const int count = teacherNamesByDay[dayIndex].size();

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

void MainWindow::addTeacherColumn()
{
    const int currentColumn = ui->scheduleTable->currentColumn();
    int dayIndex = dayIndexFromColumn(currentColumn);

    if (dayIndex < 0)
    {
        dayIndex = 0;
    }

    teacherNamesByDay[dayIndex].append("");

    rebuildScheduleTable();

    const int newColumn = firstColumnOfDay(dayIndex) + teacherNamesByDay[dayIndex].size() - 1;
    ui->scheduleTable->setCurrentCell(0, newColumn);
    loadCell(0, newColumn);
}

void MainWindow::removeTeacherColumn()
{
    const int currentColumn = ui->scheduleTable->currentColumn();
    const int dayIndex = dayIndexFromColumn(currentColumn);
    const int teacherIndex = teacherIndexFromColumn(currentColumn);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    if (teacherNamesByDay[dayIndex].size() <= 1)
    {
        teacherNamesByDay[dayIndex][0].clear();

        for (int row = 0; row < ui->scheduleTable->rowCount(); ++row)
        {
            clearEntry(row, currentColumn);
        }

        rebuildScheduleTable();

        const int column = firstColumnOfDay(dayIndex);
        ui->scheduleTable->setCurrentCell(0, column);
        loadCell(0, column);
        return;
    }

    teacherNamesByDay[dayIndex].removeAt(teacherIndex);

    rebuildScheduleTable();

    const int column = firstColumnOfDay(dayIndex);
    ui->scheduleTable->setCurrentCell(0, column);
    loadCell(0, column);
}

void MainWindow::renameTeacherColumn()
{
    const int currentColumn = ui->scheduleTable->currentColumn();
    const int dayIndex = dayIndexFromColumn(currentColumn);
    const int teacherIndex = teacherIndexFromColumn(currentColumn);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    const QString currentName = teacherNamesByDay[dayIndex][teacherIndex];

    const QString newName = ui->teacherComboBox->currentText().trimmed();

    teacherNamesByDay[dayIndex][teacherIndex] = newName;

    rebuildScheduleTable();

    const int column = firstColumnOfDay(dayIndex) + teacherIndex;
    ui->scheduleTable->setCurrentCell(0, column);
    loadCell(0, column);
}

void MainWindow::setupEditor()
{
    ui->teacherComboBox->addItems(teachers());
    ui->student1ComboBox->addItems(students());
    ui->student1GradeComboBox->addItems(grades());
    ui->student2GradeComboBox->addItems(grades());
    ui->student2ComboBox->addItems(students());
    ui->student1SubjectComboBox->addItems(subjects());
    ui->student2SubjectComboBox->addItems(subjects());

    connect(ui->scheduleTable, &QTableWidget::currentCellChanged, this, [this](int row, int column)
            { loadCell(row, column); });
    connect(ui->applyCellButton, &QPushButton::clicked, this, &MainWindow::updateCell);
    connect(ui->clearCellButton, &QPushButton::clicked, this, &MainWindow::clearSelectedCell);
    connect(ui->addTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::addTeacherColumn);
    connect(ui->removeTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::removeTeacherColumn);
    connect(ui->renameTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::renameTeacherColumn);

    // connect(ui->saveButton, &QPushButton::clicked, this, &MainWindow::saveToFile);
    // connect(ui->loadButton, &QPushButton::clicked, this, &MainWindow::loadFromFile);
}

void MainWindow::loadCell(int row, int column)
{
    selectedRow = row;
    selectedColumn = column;

    const auto *item = ui->scheduleTable->item(row, column);
    if (!item || entryIsEmpty(item))
    {
        // resetForm();
        return;
    }
    ui->teacherComboBox->setCurrentText(item->data(TeacherRole).toString());
    ui->student1ComboBox->setCurrentText(item->data(Student1NameRole).toString());
    ui->student1GradeComboBox->setCurrentText(item->data(Student1GradeRole).toString());
    ui->student1SubjectComboBox->setCurrentText(item->data(Student1SubjectRole).toString());
    ui->student2ComboBox->setCurrentText(item->data(Student2NameRole).toString());
    ui->student2GradeComboBox->setCurrentText(item->data(Student2GradeRole).toString());
    ui->student2SubjectComboBox->setCurrentText(item->data(Student2SubjectRole).toString());
    ui->student1MemoTextEdit->setPlainText(item->data(Student1MemoRole).toString());
    ui->student2MemoTextEdit->setPlainText(item->data(Student2MemoRole).toString());
}

void MainWindow::updateCell()
{
    auto *item = ui->scheduleTable->item(selectedRow, selectedColumn);
    if (!item)
    {
        return;
    }
    item->setData(TeacherRole, ui->teacherComboBox->currentText().trimmed());
    item->setData(Student1NameRole, ui->student1ComboBox->currentText().trimmed());
    item->setData(Student1GradeRole, ui->student1GradeComboBox->currentText().trimmed());
    item->setData(Student1SubjectRole, ui->student1SubjectComboBox->currentText().trimmed());
    item->setData(Student2NameRole, ui->student2ComboBox->currentText().trimmed());
    item->setData(Student2GradeRole, ui->student2GradeComboBox->currentText().trimmed());
    item->setData(Student2SubjectRole, ui->student2SubjectComboBox->currentText().trimmed());
    item->setData(Student1MemoRole, ui->student1MemoTextEdit->toPlainText().trimmed());
    item->setData(Student2MemoRole, ui->student2MemoTextEdit->toPlainText().trimmed());

    renderEntry(selectedRow, selectedColumn);
}

void MainWindow::clearSelectedCell()
{
    clearEntry(selectedRow, selectedColumn);
    // resetForm();
}

/*
void MainWindow::saveToFile()
{

}

void MainWindow::loadFromFile()
{

}
*/

void MainWindow::clearEntry(int row, int column)
{
    if (row < 0 || column < 0 || row >= ui->scheduleTable->rowCount() || column >= ui->scheduleTable->columnCount())
    {
        return;
    }

    auto *item = ui->scheduleTable->item(row, column);
    item->setData(TeacherRole, {});
    item->setData(Student1NameRole, {});
    item->setData(Student1GradeRole, {});
    item->setData(Student1SubjectRole, {});
    item->setData(Student2NameRole, {});
    item->setData(Student2GradeRole, {});
    item->setData(Student2SubjectRole, {});
    item->setData(Student1MemoRole, {});
    item->setData(Student2MemoRole, {});
    item->setText({});
}

void MainWindow::renderEntry(int row, int column)
{
    auto *item = ui->scheduleTable->item(row, column);
    if (!item)
    {
        return;
    }
    item->setText(cellTextFromItem(item));
}

bool MainWindow::entryIsEmpty(const QTableWidgetItem *item) const
{
    return item->data(TeacherRole).toString().trimmed().isEmpty() && item->data(Student1NameRole).toString().trimmed().isEmpty() && item->data(Student1GradeRole).toString().trimmed().isEmpty() && item->data(Student1SubjectRole).toString().trimmed().isEmpty() && item->data(Student2NameRole).toString().trimmed().isEmpty() && item->data(Student2GradeRole).toString().trimmed().isEmpty() && item->data(Student2SubjectRole).toString().trimmed().isEmpty() && item->data(Student1MemoRole).toString().trimmed().isEmpty() && item->data(Student2MemoRole).toString().trimmed().isEmpty();
}

QString MainWindow::cellTextFromItem(const QTableWidgetItem *item) const
{
    QStringList lines;
    const QString student1 = studentLine(item->data(Student1NameRole).toString(),
                                         item->data(Student1GradeRole).toString(),
                                         item->data(Student1SubjectRole).toString());
    const QString student2 = studentLine(item->data(Student2NameRole).toString(),
                                         item->data(Student2GradeRole).toString(),
                                         item->data(Student2SubjectRole).toString());
    const QString student1Memo = item->data(Student1MemoRole).toString().trimmed();
    const QString student2Memo = item->data(Student2MemoRole).toString().trimmed();

    if (!student1.isEmpty())
    {
        lines << student1;
    }
    if (!student2.isEmpty())
    {
        lines << student2;
    }

    return lines.join('\n');
}
