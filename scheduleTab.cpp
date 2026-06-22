#include "mainwindow.h"
#include "ui_mainwindow.h"

namespace
{
    QStringList gradeslist()
    {
        return {"", "小1", "小2", "小3", "小4", "小5", "小6", "中1", "中2", "中3", "高1", "高2", "高3", "既卒"};
    }

    QStringList subjectslist()
    {
        return {"", "英語", "数学", "国語", "理科", "社会", "算数", "理社", "その他"};
    }

    QStringList teacherslist()
    {
        return {"", "講師1", "講師2", "講師3", "講師4"};
    }

    QStringList studentslist()
    {
        return {"", "生徒1", "生徒2", "生徒3", "生徒4", "生徒5"};
    }
}

void MainWindow::setupTable()
{
    initializeTable();
    renderTable();
}

void MainWindow::setupEditor()
{
    ui->teacherComboBox->addItems(teacherslist());
    ui->student1ComboBox->addItems(studentslist());
    ui->student1GradeComboBox->addItems(gradeslist());
    ui->student1SubjectComboBox->addItems(subjectslist());
    ui->student2ComboBox->addItems(studentslist());
    ui->student2GradeComboBox->addItems(gradeslist());
    ui->student2SubjectComboBox->addItems(subjectslist());

    connect(ui->scheduleTable, &QTableWidget::currentCellChanged, this, [this](int row, int column)
            { loadCell(row, column); });
    connect(ui->applyCellButton, &QPushButton::clicked, this, &MainWindow::updateCell);
    connect(ui->clearCellButton, &QPushButton::clicked, this, &MainWindow::clearCell);
    connect(ui->addTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::addTeacherColumn);
    connect(ui->removeTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::removeTeacherColumn);
    connect(ui->renameTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::renameTeacherColumn);
    connect(ui->copyButton, &QPushButton::clicked, this, &MainWindow::copyCell);
    connect(ui->pasteButton, &QPushButton::clicked, this, &MainWindow::pasteCell);

    // connect(ui->saveButton, &QPushButton::clicked, this, &MainWindow::saveToFile);
    // connect(ui->loadButton, &QPushButton::clicked, this, &MainWindow::loadFromFile);
}

void MainWindow::initializeTable()
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

void MainWindow::renderTable()
{
    QStringList headers;
    int totalColumns = 0;

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

            ++totalColumns;
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

    for (int row = 0; row < periods.size(); ++row)
    {
        for (int column = 0; column < totalColumns; ++column)
        {
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
            ui->scheduleTable->setItem(row, column, item);

            renderCell(row, column);
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
    int dayIndex = dayIndexFromColumn(selectedColumn);

    if (dayIndex < 0 || dayIndex >= schedule.size())
    {
        dayIndex = 0;
    }

    TeacherColumn newColumn;
	newColumn.teacherName = "";
	newColumn.lessons.resize(periods.size());

	schedule[dayIndex].append(newColumn);

    renderTable();

    const int newColumnIndex = firstColumnOfDay(dayIndex) + schedule[dayIndex].size() - 1;
    ui->scheduleTable->setCurrentCell(0, newColumnIndex);
    loadCell(0, newColumnIndex);
}

void MainWindow::removeTeacherColumn()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    if (schedule[dayIndex].size() <= 1)
    {
        schedule[dayIndex][0].teacherName.clear();
        schedule[dayIndex][teacherIndex].lessons.clear();
        schedule[dayIndex][teacherIndex].lessons.resize(periods.size());
        renderTable();

        const int column = firstColumnOfDay(dayIndex);
        ui->scheduleTable->setCurrentCell(0, column);
        loadCell(0, column);
        return;
    }

    schedule[dayIndex].removeAt(teacherIndex);

    renderTable();

    const int newSelectedColumn = firstColumnOfDay(dayIndex);
    ui->scheduleTable->setCurrentCell(0, newSelectedColumn);
    loadCell(0, newSelectedColumn);
}

void MainWindow::renameTeacherColumn()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    const QString currentName = schedule[dayIndex][teacherIndex].teacherName;

    const QString newName = ui->teacherComboBox->currentText().trimmed();

    schedule[dayIndex][teacherIndex].teacherName = newName;

    renderTable();

    const int column = firstColumnOfDay(dayIndex) + teacherIndex;
    ui->scheduleTable->setCurrentCell(0, column);
    loadCell(0, column);
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

    renderEntry();
}

void MainWindow::updateCell()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);

    if (selectedRow < 0 || selectedRow >= periods.size() || dayIndex < 0 || teacherIndex < 0)
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

    schedule[dayIndex][teacherIndex].lessons[selectedRow] = lesson;

    renderCell(selectedRow, selectedColumn);
}

QString MainWindow::cellTextFromData(const CellData &lesson) const
{
	QStringList lines;

	const QString student1Info = QString("%1 %2")
		.arg(lesson.student1Grade.trimmed(), lesson.student1Subject.trimmed());
	const QString student1Name = lesson.student1Name.trimmed();

	const QString student2Info = QString("%1 %2")
		.arg(lesson.student2Grade.trimmed(), lesson.student2Subject.trimmed());
	const QString student2Name = lesson.student2Name.trimmed();

    if (!student1Info.isEmpty()) lines << student1Info;
    if (!student1Name.isEmpty()) lines << student1Name;
    if (!student2Info.isEmpty()) lines << student2Info;
    if (!student2Name.isEmpty()) lines << student2Name;

    return lines.join('\n');
}

void MainWindow::renderCell(int row, int column)
{
    if (row < 0 || column < 0 || row >= periods.size() || column >= ui->scheduleTable->columnCount())
    {
        return;
    }

    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    auto lesson = schedule[dayIndex][teacherIndex].lessons[row];
    auto *item = ui->scheduleTable->item(row, column);
    if (item)
    {
        item->setText(cellTextFromData(lesson));
    }
}

void MainWindow::clearCell()
{
    if (selectedRow < 0 || selectedColumn < 0 || selectedRow >= ui->scheduleTable->rowCount() || selectedColumn >= ui->scheduleTable->columnCount())
    {
        return;
    }

    auto *lesson = &schedule[dayIndexFromColumn(selectedColumn)][teacherIndexFromColumn(selectedColumn)].lessons[selectedRow];
    lesson->student1Name.clear();
    lesson->student1Grade.clear();
    lesson->student1Subject.clear();
    lesson->student2Name.clear();
    lesson->student2Grade.clear();
    lesson->student2Subject.clear();
    lesson->student1Memo.clear();
    lesson->student2Memo.clear();

    renderEntry();
    renderCell(selectedRow, selectedColumn);
}

void MainWindow::renderEntry()
{
    if (selectedRow < 0 || selectedColumn < 0 || selectedRow >= ui->scheduleTable->rowCount() || selectedColumn >= ui->scheduleTable->columnCount())
    {
        return;
    }
    auto teacherColumn = schedule[dayIndexFromColumn(selectedColumn)][teacherIndexFromColumn(selectedColumn)];
    auto lesson = teacherColumn.lessons[selectedRow];

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

void MainWindow::copyCell()
{
    QString json = lessonToJson(selectedRow, selectedColumn);

    QApplication::clipboard()->setText(json);

    statusBar()->showMessage("コピーしました", 2000);
}

void MainWindow::pasteCell()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);

    if (selectedRow < 0 || selectedRow >= periods.size() || dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    schedule[dayIndex][teacherIndex].lessons[selectedRow] = jsonToLesson(QApplication::clipboard()->text());

    renderCell(selectedRow, selectedColumn);
    loadCell(selectedRow, selectedColumn);

    statusBar()->showMessage("貼り付けました", 2000);
}

bool MainWindow::celldataIsEmpty(const CellData &lesson) const
{
    return lesson.student1Name.trimmed().isEmpty() && lesson.student1Grade.trimmed().isEmpty() && lesson.student1Subject.trimmed().isEmpty() && lesson.student2Name.trimmed().isEmpty() && lesson.student2Grade.trimmed().isEmpty() && lesson.student2Subject.trimmed().isEmpty() && lesson.student1Memo.trimmed().isEmpty() && lesson.student2Memo.trimmed().isEmpty();
}

