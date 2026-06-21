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

namespace
{
    constexpr int TeachersPerDay = 4;

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
        // return parts.join(" / ");
        return grade + " " + subject + " " + name;
    }
}

void MainWindow::setupTable()
{
    const QStringList days = {"月", "火", "水", "木", "金", "土"};
    const QStringList periods = {
        "14:40-15:50",
        "15:50-17:00",
        "17:00-18:10",
        "18:10-19:20",
        "19:20-20:30",
        "20:30-21:40"
    };

    QStringList headers;
    for (const QString &day : days) {
        for (int teacher = 1; teacher <= TeachersPerDay; ++teacher) {
            headers << QString("%1\n講師%2").arg(day).arg(teacher);
        }
    }

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

    for (int row = 0; row < ui->scheduleTable->rowCount(); ++row) {
        for (int column = 0; column < ui->scheduleTable->columnCount(); ++column) {
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
            ui->scheduleTable->setItem(row, column, item);
        }
    }
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

    connect(ui->scheduleTable, &QTableWidget::currentCellChanged, this, [this](int row, int column) {
        loadCell(row, column);
    });
    connect(ui->applyCellButton, &QPushButton::clicked, this, &MainWindow::updateCell);
    connect(ui->clearCellButton, &QPushButton::clicked, this, &MainWindow::clearSelectedCell);
    //connect(ui->saveButton, &QPushButton::clicked, this, &MainWindow::saveToFile);
    //connect(ui->loadButton, &QPushButton::clicked, this, &MainWindow::loadFromFile);
}

void MainWindow::loadCell(int row, int column)
{
    selectedRow = row;
    selectedColumn = column;

    const auto *item = ui->scheduleTable->item(row, column);
    if (!item || entryIsEmpty(item)) {
        //resetForm();
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
    if (!item) {
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
    //resetForm();
}

/*
void MainWindow::saveToFile()
{
    const QString fileName = QFileDialog::getSaveFileName(this, "時間割を保存", {}, "時間割 (*.json)");
    if (fileName.isEmpty()) {
        return;
    }

    QJsonArray cells;
    for (int row = 0; row < ui->scheduleTable->rowCount(); ++row) {
        for (int column = 0; column < ui->scheduleTable->columnCount(); ++column) {
            const auto *item = ui->scheduleTable->item(row, column);
            if (!item || entryIsEmpty(item)) {
                continue;
            }

            QJsonObject cell;
            cell["row"] = row;
            cell["column"] = column;
            cell["day"] = column / TeachersPerDay;
            cell["teacherSlot"] = column % TeachersPerDay;
            cell["teacher"] = item->data(TeacherRole).toString();
            cell["room"] = item->data(RoomRole).toString();
            cell["student1"] = item->data(Student1NameRole).toString();
            cell["grade1"] = item->data(Student1GradeRole).toString();
            cell["subject1"] = item->data(Student1SubjectRole).toString();
            cell["student2"] = item->data(Student2NameRole).toString();
            cell["grade2"] = item->data(Student2GradeRole).toString();
            cell["subject2"] = item->data(Student2SubjectRole).toString();
            cell["extraStudents"] = item->data(ExtraStudentsRole).toString();
            cell["memo"] = item->data(MemoRole).toString();
            cells.append(cell);
        }
    }

    QJsonObject root;
    root["cells"] = cells;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "保存できません", "ファイルを保存できませんでした。");
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    statusBar()->showMessage("時間割を保存しました", 3000);
}

void MainWindow::loadFromFile()
{
    const QString fileName = QFileDialog::getOpenFileName(this, "時間割を開く", {}, "時間割 (*.json)");
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "開けません", "ファイルを開けませんでした。");
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonArray cells = document.object().value("cells").toArray();

    for (int row = 0; row < ui->scheduleTable->rowCount(); ++row) {
        for (int column = 0; column < ui->scheduleTable->columnCount(); ++column) {
            clearEntry(row, column);
        }
    }

    for (const QJsonValue &value : cells) {
        const QJsonObject cell = value.toObject();
        const int row = cell.value("row").toInt(-1);
        const int column = cell.contains("column")
                               ? cell.value("column").toInt(-1)
                               : cell.value("day").toInt(-1) * TeachersPerDay
                                     + cell.value("teacherSlot").toInt(0);
        if (row < 0 || row >= ui->scheduleTable->rowCount() || column < 0
            || column >= ui->scheduleTable->columnCount()) {
            continue;
        }

        auto *item = ui->scheduleTable->item(row, column);
        item->setData(TeacherRole, cell.value("teacher").toString());
        item->setData(RoomRole, cell.value("room").toString());
        item->setData(Student1NameRole, cell.value("student1").toString(cell.value("student").toString()));
        item->setData(Student1GradeRole, cell.value("grade1").toString());
        item->setData(Student1SubjectRole, cell.value("subject1").toString(cell.value("subject").toString()));
        item->setData(Student2NameRole, cell.value("student2").toString());
        item->setData(Student2GradeRole, cell.value("grade2").toString());
        item->setData(Student2SubjectRole, cell.value("subject2").toString());
        item->setData(ExtraStudentsRole, cell.value("extraStudents").toString());
        item->setData(MemoRole, cell.value("memo").toString());
        renderEntry(row, column);
    }

    loadCell(ui->scheduleTable->currentRow(), ui->scheduleTable->currentColumn());
    statusBar()->showMessage("時間割を開きました", 3000);
}

void MainWindow::resetForm()
{
    teacherEdit->clear();
    roomEdit->clear();
    student1Edit->clear();
    grade1Box->setCurrentIndex(0);
    subject1Box->setCurrentIndex(0);
    student2Edit->clear();
    grade2Box->setCurrentIndex(0);
    subject2Box->setCurrentIndex(0);
    extraStudentsEdit->clear();
    memoEdit->clear();
}
*/

void MainWindow::clearEntry(int row, int column)
{
    if (row < 0 || column < 0 || row >= ui->scheduleTable->rowCount()
        || column >= ui->scheduleTable->columnCount()) {
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
    if (!item) {
        return;
    }
    item->setText(cellTextFromItem(item));
}

bool MainWindow::entryIsEmpty(const QTableWidgetItem *item) const
{
    return item->data(TeacherRole).toString().trimmed().isEmpty()
        && item->data(Student1NameRole).toString().trimmed().isEmpty()
        && item->data(Student1GradeRole).toString().trimmed().isEmpty()
        && item->data(Student1SubjectRole).toString().trimmed().isEmpty()
        && item->data(Student2NameRole).toString().trimmed().isEmpty()
        && item->data(Student2GradeRole).toString().trimmed().isEmpty()
        && item->data(Student2SubjectRole).toString().trimmed().isEmpty()
        && item->data(Student1MemoRole).toString().trimmed().isEmpty()
        && item->data(Student2MemoRole).toString().trimmed().isEmpty();
}

QString MainWindow::cellTextFromItem(const QTableWidgetItem *item) const
{
    QStringList lines;
    const QString teacher = item->data(TeacherRole).toString().trimmed();
    const QString student1 = studentLine(item->data(Student1NameRole).toString(),
                                         item->data(Student1GradeRole).toString(),
                                         item->data(Student1SubjectRole).toString());
    const QString student2 = studentLine(item->data(Student2NameRole).toString(),
                                         item->data(Student2GradeRole).toString(),
                                         item->data(Student2SubjectRole).toString());
    const QString student1Memo = item->data(Student1MemoRole).toString().trimmed();
    const QString student2Memo = item->data(Student2MemoRole).toString().trimmed();

    if (!teacher.isEmpty()) {
        lines << "講師: " + teacher;
    }
    if (!student1.isEmpty() || !student2.isEmpty() || !student1Memo.isEmpty() || !student2Memo.isEmpty()) {
        lines << "生徒:";
    }
    if (!student1.isEmpty()) {
        lines << "  1. " + student1;
    }
    if (!student2.isEmpty()) {
        lines << "  2. " + student2;
    }
    if (!student1Memo.isEmpty()) {
        lines << "メモ: " + student1Memo;
    }
    if (!student2Memo.isEmpty()) {
        lines << "メモ: " + student2Memo;
    }

    return lines.join('\n');
}
