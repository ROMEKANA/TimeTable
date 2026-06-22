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
    int cellDefaultSectionSize = 145;

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
    initializeSchedule();
    rebuildScheduleTable();
}

void MainWindow::initializeSchedule()
{
	schedule.clear();

	for (int i = 0; i < days.size(); ++i)
	{
		TeacherColumn emptyColumn;
		emptyColumn.teacherName = "";
		emptyColumn.lessons.resize(periods.size());

		schedule.append(QVector<TeacherColumn>{emptyColumn});
	}
}

void MainWindow::rebuildScheduleTable()
{
    QStringList headers;

    for (int dayIndex = 0; dayIndex < schedule.size(); ++dayIndex)
    {
        for (int teacherIndex = 0; teacherIndex < schedule[dayIndex].size(); ++teacherIndex)
        {
            const QString teacherName = schedule[dayIndex][teacherIndex].teacherName.trimmed();

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
    ui->scheduleTable->horizontalHeader()->setDefaultSectionSize(cellDefaultSectionSize);
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
        column += schedule[i].size();
    }

    return column;
}

int MainWindow::columnCountOfDay(int dayIndex) const
{
    if (dayIndex < 0 || dayIndex >= schedule.size())
    {
        return 0;
    }

    return schedule[dayIndex].size();
}

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

    schedule[dayIndex].append(TeacherColumn{});

    rebuildScheduleTable();

    const int newColumn = firstColumnOfDay(dayIndex) + schedule[dayIndex].size() - 1;
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

    if (schedule[dayIndex].size() <= 1)
    {
        schedule[dayIndex][0].teacherName.clear();

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

    schedule[dayIndex].removeAt(teacherIndex);

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

    const QString currentName = schedule[dayIndex][teacherIndex].teacherName;

    const QString newName = ui->teacherComboBox->currentText().trimmed();

    schedule[dayIndex][teacherIndex].teacherName = newName;

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

	const int dayIndex = dayIndexFromColumn(column);
	const int teacherIndex = teacherIndexFromColumn(column);

	if (row < 0 || row >= periods.size() || dayIndex < 0 || teacherIndex < 0)
	{
		return;
	}

	const TeacherColumn &teacherColumn = schedule[dayIndex][teacherIndex];
	const CellData &lesson = teacherColumn.lessons[row];

	ui->teacherComboBox->setCurrentText(teacherColumn.teacherName);
	ui->student1ComboBox->setCurrentText(lesson.student1Name);
	ui->student1GradeComboBox->setCurrentText(lesson.student1Grade);
	ui->student1SubjectComboBox->setCurrentText(lesson.student1Subject);
	ui->student2ComboBox->setCurrentText(lesson.student2Name);
	ui->student2GradeComboBox->setCurrentText(lesson.student2Grade);
	ui->student2SubjectComboBox->setCurrentText(lesson.student2Subject);
	ui->student1MemoTextEdit->setPlainText(lesson.student1Memo);
	ui->student2MemoTextEdit->setPlainText(lesson.student2Memo);
}

void MainWindow::updateCell()
{
    if (selectedRow < 0 || selectedColumn < 0)
    {
        return;
    }

    CellData lesson;

    lesson.student1Name = ui->student1ComboBox->currentText();
    lesson.student1Grade = ui->student1GradeComboBox->currentText();
    lesson.student1Subject = ui->student1SubjectComboBox->currentText();
    lesson.student2Name = ui->student2ComboBox->currentText();
    lesson.student2Grade = ui->student2GradeComboBox->currentText();
    lesson.student2Subject = ui->student2SubjectComboBox->currentText();
    lesson.student1Memo = ui->student1MemoTextEdit->toPlainText();
    lesson.student2Memo = ui->student2MemoTextEdit->toPlainText();
    
    schedule[dayIndexFromColumn(selectedColumn)][teacherIndexFromColumn(selectedColumn)].lessons[selectedRow] = lesson;

    renderEntry(selectedRow, selectedColumn);
}

void MainWindow::clearSelectedCell()
{
    clearEntry(selectedRow, selectedColumn);
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

    auto *lesson = &schedule[dayIndexFromColumn(column)][teacherIndexFromColumn(column)].lessons[row];
    lesson->student1Name.clear();
    lesson->student1Grade.clear();
    lesson->student1Subject.clear();
    lesson->student2Name.clear();
    lesson->student2Grade.clear();
    lesson->student2Subject.clear();
    lesson->student1Memo.clear();
    lesson->student2Memo.clear();
}

void MainWindow::renderEntry(int row, int column)
{
    auto *lesson = &schedule[dayIndexFromColumn(column)][teacherIndexFromColumn(column)].lessons[row - 1];

    rebuildScheduleTable();
}

bool MainWindow::entryIsEmpty(const CellData &lesson) const
{
    return lesson.student1Name.trimmed().isEmpty() && lesson.student1Grade.trimmed().isEmpty() && lesson.student1Subject.trimmed().isEmpty() && lesson.student2Name.trimmed().isEmpty() && lesson.student2Grade.trimmed().isEmpty() && lesson.student2Subject.trimmed().isEmpty() && lesson.student1Memo.trimmed().isEmpty() && lesson.student2Memo.trimmed().isEmpty();
}

QString MainWindow::cellTextFromData(const CellData &lesson) const
{
    QStringList lines;
    const QString student1 = studentLine(lesson.student1Name, lesson.student1Grade, lesson.student1Subject);
    const QString student2 = studentLine(lesson.student2Name, lesson.student2Grade, lesson.student2Subject);
    //const QString student1Memo = lesson.student1Memo.trimmed();
    //const QString student2Memo = lesson.student2Memo.trimmed();

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
