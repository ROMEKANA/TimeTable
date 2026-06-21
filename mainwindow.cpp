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

namespace {
constexpr int TeachersPerDay = 4;

enum DataRole {
    TeacherRole = Qt::UserRole,
    RoomRole,
    Student1NameRole,
    Student1GradeRole,
    Student1SubjectRole,
    Student2NameRole,
    Student2GradeRole,
    Student2SubjectRole,
    ExtraStudentsRole,
    MemoRole
};

QStringList grades()
{
    return {"", "小1", "小2", "小3", "小4", "小5", "小6", "中1", "中2", "中3", "高1", "高2", "高3", "既卒"};
}

QStringList subjects()
{
    return {"", "英語", "数学", "国語", "理科", "社会", "算数", "自習", "面談", "その他"};
}

QString studentLine(const QString &name, const QString &grade, const QString &subject)
{
    QStringList parts;
    if (!name.trimmed().isEmpty()) {
        parts << name.trimmed();
    }
    if (!grade.trimmed().isEmpty()) {
        parts << grade.trimmed();
    }
    if (!subject.trimmed().isEmpty()) {
        parts << subject.trimmed();
    }
    return parts.join(" / ");
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("塾時間割表");
    resize(1500, 760);

    setupTable();
    setupEditor();
    ui->tableView->setCurrentCell(0, 0);
    loadCell(0, 0);
}

MainWindow::~MainWindow()
{
    delete ui;
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

    ui->tableView->setColumnCount(headers.size());
    ui->tableView->setRowCount(periods.size());
    ui->tableView->setHorizontalHeaderLabels(headers);
    ui->tableView->setVerticalHeaderLabels(periods);

    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setDefaultSectionSize(145);
    ui->tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->tableView->setWordWrap(true);

    for (int row = 0; row < ui->tableView->rowCount(); ++row) {
        for (int column = 0; column < ui->tableView->columnCount(); ++column) {
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
            ui->tableView->setItem(row, column, item);
        }
    }
}

void MainWindow::setupEditor()
{
    auto *rootLayout = new QHBoxLayout(ui->centralwidget);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);

    auto *editorPanel = new QWidget(ui->centralwidget);
    editorPanel->setMinimumWidth(320);
    editorPanel->setMaximumWidth(400);

    auto *editorLayout = new QVBoxLayout(editorPanel);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(10);

    auto *titleLabel = new QLabel("選択した講師枠", editorPanel);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: 600;");
    editorLayout->addWidget(titleLabel);

    auto *formLayout = new QFormLayout;
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignTop);

    teacherEdit = new QLineEdit(editorPanel);
    roomEdit = new QLineEdit(editorPanel);
    student1Edit = new QLineEdit(editorPanel);
    grade1Box = new QComboBox(editorPanel);
    subject1Box = new QComboBox(editorPanel);
    student2Edit = new QLineEdit(editorPanel);
    grade2Box = new QComboBox(editorPanel);
    subject2Box = new QComboBox(editorPanel);
    extraStudentsEdit = new QTextEdit(editorPanel);
    memoEdit = new QLineEdit(editorPanel);

    grade1Box->addItems(grades());
    grade2Box->addItems(grades());
    subject1Box->addItems(subjects());
    subject2Box->addItems(subjects());
    extraStudentsEdit->setFixedHeight(76);
    extraStudentsEdit->setPlaceholderText("例: 田中 中2 英語\n佐藤 高1 数学");

    formLayout->addRow("講師", teacherEdit);
    formLayout->addRow("教室", roomEdit);
    formLayout->addRow("生徒1", student1Edit);
    formLayout->addRow("生徒1 学年", grade1Box);
    formLayout->addRow("生徒1 教科", subject1Box);
    formLayout->addRow("生徒2", student2Edit);
    formLayout->addRow("生徒2 学年", grade2Box);
    formLayout->addRow("生徒2 教科", subject2Box);
    formLayout->addRow("追加生徒", extraStudentsEdit);
    formLayout->addRow("メモ", memoEdit);
    editorLayout->addLayout(formLayout);

    applyButton = new QPushButton("表に反映", editorPanel);
    clearButton = new QPushButton("この講師枠を空にする", editorPanel);
    auto *saveButton = new QPushButton("保存", editorPanel);
    auto *loadButton = new QPushButton("開く", editorPanel);

    editorLayout->addWidget(applyButton);
    editorLayout->addWidget(clearButton);
    editorLayout->addSpacing(12);
    editorLayout->addWidget(saveButton);
    editorLayout->addWidget(loadButton);
    editorLayout->addStretch();

    rootLayout->addWidget(ui->tableView, 1);
    rootLayout->addWidget(editorPanel);

    connect(ui->tableView, &QTableWidget::currentCellChanged, this, [this](int row, int column) {
        loadCell(row, column);
    });
    connect(applyButton, &QPushButton::clicked, this, &MainWindow::updateCell);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearSelectedCell);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveToFile);
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadFromFile);
}

void MainWindow::loadCell(int row, int column)
{
    selectedRow = row;
    selectedColumn = column;

    const auto *item = ui->tableView->item(row, column);
    if (!item || entryIsEmpty(item)) {
        resetForm();
        return;
    }

    teacherEdit->setText(item->data(TeacherRole).toString());
    roomEdit->setText(item->data(RoomRole).toString());
    student1Edit->setText(item->data(Student1NameRole).toString());
    grade1Box->setCurrentText(item->data(Student1GradeRole).toString());
    subject1Box->setCurrentText(item->data(Student1SubjectRole).toString());
    student2Edit->setText(item->data(Student2NameRole).toString());
    grade2Box->setCurrentText(item->data(Student2GradeRole).toString());
    subject2Box->setCurrentText(item->data(Student2SubjectRole).toString());
    extraStudentsEdit->setPlainText(item->data(ExtraStudentsRole).toString());
    memoEdit->setText(item->data(MemoRole).toString());
}

void MainWindow::updateCell()
{
    auto *item = ui->tableView->item(selectedRow, selectedColumn);
    if (!item) {
        return;
    }

    item->setData(TeacherRole, teacherEdit->text().trimmed());
    item->setData(RoomRole, roomEdit->text().trimmed());
    item->setData(Student1NameRole, student1Edit->text().trimmed());
    item->setData(Student1GradeRole, grade1Box->currentText().trimmed());
    item->setData(Student1SubjectRole, subject1Box->currentText().trimmed());
    item->setData(Student2NameRole, student2Edit->text().trimmed());
    item->setData(Student2GradeRole, grade2Box->currentText().trimmed());
    item->setData(Student2SubjectRole, subject2Box->currentText().trimmed());
    item->setData(ExtraStudentsRole, extraStudentsEdit->toPlainText().trimmed());
    item->setData(MemoRole, memoEdit->text().trimmed());

    renderEntry(selectedRow, selectedColumn);
}

void MainWindow::clearSelectedCell()
{
    clearEntry(selectedRow, selectedColumn);
    resetForm();
}

void MainWindow::saveToFile()
{
    const QString fileName = QFileDialog::getSaveFileName(this, "時間割を保存", {}, "時間割 (*.json)");
    if (fileName.isEmpty()) {
        return;
    }

    QJsonArray cells;
    for (int row = 0; row < ui->tableView->rowCount(); ++row) {
        for (int column = 0; column < ui->tableView->columnCount(); ++column) {
            const auto *item = ui->tableView->item(row, column);
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

    for (int row = 0; row < ui->tableView->rowCount(); ++row) {
        for (int column = 0; column < ui->tableView->columnCount(); ++column) {
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
        if (row < 0 || row >= ui->tableView->rowCount() || column < 0
            || column >= ui->tableView->columnCount()) {
            continue;
        }

        auto *item = ui->tableView->item(row, column);
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

    loadCell(ui->tableView->currentRow(), ui->tableView->currentColumn());
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

void MainWindow::clearEntry(int row, int column)
{
    if (row < 0 || column < 0 || row >= ui->tableView->rowCount()
        || column >= ui->tableView->columnCount()) {
        return;
    }

    auto *item = ui->tableView->item(row, column);
    item->setData(TeacherRole, {});
    item->setData(RoomRole, {});
    item->setData(Student1NameRole, {});
    item->setData(Student1GradeRole, {});
    item->setData(Student1SubjectRole, {});
    item->setData(Student2NameRole, {});
    item->setData(Student2GradeRole, {});
    item->setData(Student2SubjectRole, {});
    item->setData(ExtraStudentsRole, {});
    item->setData(MemoRole, {});
    item->setText({});
}

void MainWindow::renderEntry(int row, int column)
{
    auto *item = ui->tableView->item(row, column);
    if (!item) {
        return;
    }
    item->setText(cellTextFromItem(item));
}

bool MainWindow::entryIsEmpty(const QTableWidgetItem *item) const
{
    return item->data(TeacherRole).toString().trimmed().isEmpty()
        && item->data(RoomRole).toString().trimmed().isEmpty()
        && item->data(Student1NameRole).toString().trimmed().isEmpty()
        && item->data(Student1GradeRole).toString().trimmed().isEmpty()
        && item->data(Student1SubjectRole).toString().trimmed().isEmpty()
        && item->data(Student2NameRole).toString().trimmed().isEmpty()
        && item->data(Student2GradeRole).toString().trimmed().isEmpty()
        && item->data(Student2SubjectRole).toString().trimmed().isEmpty()
        && item->data(ExtraStudentsRole).toString().trimmed().isEmpty()
        && item->data(MemoRole).toString().trimmed().isEmpty();
}

QString MainWindow::cellTextFromItem(const QTableWidgetItem *item) const
{
    QStringList lines;
    const QString teacher = item->data(TeacherRole).toString().trimmed();
    const QString room = item->data(RoomRole).toString().trimmed();
    const QString student1 = studentLine(item->data(Student1NameRole).toString(),
                                         item->data(Student1GradeRole).toString(),
                                         item->data(Student1SubjectRole).toString());
    const QString student2 = studentLine(item->data(Student2NameRole).toString(),
                                         item->data(Student2GradeRole).toString(),
                                         item->data(Student2SubjectRole).toString());
    const QString extraStudents = item->data(ExtraStudentsRole).toString().trimmed();
    const QString memo = item->data(MemoRole).toString().trimmed();

    if (!teacher.isEmpty()) {
        lines << "講師: " + teacher;
    }
    if (!room.isEmpty()) {
        lines << "教室: " + room;
    }
    if (!student1.isEmpty() || !student2.isEmpty() || !extraStudents.isEmpty()) {
        lines << "生徒:";
    }
    if (!student1.isEmpty()) {
        lines << "  1. " + student1;
    }
    if (!student2.isEmpty()) {
        lines << "  2. " + student2;
    }
    if (!extraStudents.isEmpty()) {
        const QStringList extraLines = extraStudents.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : extraLines) {
            lines << "  - " + line.trimmed();
        }
    }
    if (!memo.isEmpty()) {
        lines << "メモ: " + memo;
    }

    return lines.join('\n');
}
