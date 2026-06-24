#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::setupTable()
{
    initializeTable();
    renderTable();

    scheduleTabConnects();

    loadLatestSchedule();
}

void MainWindow::scheduleTabConnects()
{
    connect(ui->scheduleTable, &QTableWidget::currentCellChanged, this, [this](int row, int column)
            { loadCell(row, column); });
    connect(ui->applyCellButton, &QPushButton::clicked, this, &MainWindow::updateCell);
    connect(ui->clearCellButton, &QPushButton::clicked, this, &MainWindow::clearCell);
    connect(ui->addTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::addTeacherColumn);
    connect(ui->removeTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::removeTeacherColumn);
    connect(ui->renameTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::renameTeacherColumn);
    connect(ui->copyButton, &QPushButton::clicked, this, &MainWindow::copyCell);
    connect(ui->pasteButton, &QPushButton::clicked, this, &MainWindow::pasteCell);

    connect(ui->saveScheduleButton, &QPushButton::clicked, this, &MainWindow::saveScheduleToFile);
    connect(ui->loadScheduleButton, &QPushButton::clicked, this, &MainWindow::loadScheduleButton);

    connect(
        ui->student1GradeComboBox,
        &QComboBox::currentTextChanged,
        this,
        [this](const QString &grade)
        {
            updateStudentComboBox(ui->student1ComboBox, grade);
        });

    connect(
        ui->student2GradeComboBox,
        &QComboBox::currentTextChanged,
        this,
        [this](const QString &grade)
        {
            updateStudentComboBox(ui->student2ComboBox, grade);
        });
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
    // いない学年は表示させないようにいつかする
    ui->student1GradeComboBox->addItems(grades);
    ui->student2GradeComboBox->addItems(grades);

    updateTeacherComboBox(ui->teacherComboBox);
    ui->student1SubjectComboBox->addItems(subjects);
    ui->student2SubjectComboBox->addItems(subjects);

    updateStudentComboBox(
        ui->student1ComboBox,
        ui->student1GradeComboBox->currentText());

    updateStudentComboBox(
        ui->student2ComboBox,
        ui->student2GradeComboBox->currentText());

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

    if (!student1Info.isEmpty())
        lines << student1Info;
    if (!student1Name.isEmpty())
        lines << student1Name;
    if (!student2Info.isEmpty())
        lines << student2Info;
    if (!student2Name.isEmpty())
        lines << student2Name;

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

void MainWindow::updateStudentComboBox(QComboBox *comboBox, const QString &grade)
{
    QString currentName = comboBox->currentText();

    comboBox->clear();
    comboBox->addItem("");

    for (const GradeStudents &gradeStudents : allStudents)
    {
        if (gradeStudents.Grade != grade)
        {
            continue;
        }

        for (const StudentData &student : gradeStudents.students)
        {
            comboBox->addItem(student.Name);
        }

        break;
    }

    int index = comboBox->findText(currentName);

    if (index >= 0)
    {
        comboBox->setCurrentIndex(index);
    }
}

void MainWindow::updateTeacherComboBox(QComboBox *comboBox)
{
    QString currentName = comboBox->currentText();

    comboBox->clear();
    comboBox->addItem("");

    for (const TeacherData &teacher : teachers)
    {
        comboBox->addItem(teacher.name);
    }

    int index = comboBox->findText(currentName);

    if (index >= 0)
    {
        comboBox->setCurrentIndex(index);
    }
}

QString MainWindow::lessonToJson(const CellData &lesson) const
{
    QJsonObject object;
    object["student1Name"] = lesson.student1Name;
    object["student1Grade"] = lesson.student1Grade;
    object["student1Subject"] = lesson.student1Subject;
    object["student2Name"] = lesson.student2Name;
    object["student2Grade"] = lesson.student2Grade;
    object["student2Subject"] = lesson.student2Subject;
    object["student1Memo"] = lesson.student1Memo;
    object["student2Memo"] = lesson.student2Memo;

    QJsonDocument document(object);

    return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
}

QString MainWindow::lessonToJson(const int row, const int column) const
{
    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);

    if (row < 0 || row >= periods.size() || dayIndex < 0 || teacherIndex < 0)
    {
        return QString();
    }

    return lessonToJson(schedule[dayIndex][teacherIndex].lessons[row]);
}

CellData MainWindow::jsonToLesson(const QString &json) const
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject())
    {
        statusBar()->showMessage("貼り付けできるセルデータではありません", 2000);
        return CellData();
    }

    const QJsonObject object = document.object();

    CellData lesson;
    lesson.student1Name = object.value("student1Name").toString();
    lesson.student1Grade = object.value("student1Grade").toString();
    lesson.student1Subject = object.value("student1Subject").toString();
    lesson.student2Name = object.value("student2Name").toString();
    lesson.student2Grade = object.value("student2Grade").toString();
    lesson.student2Subject = object.value("student2Subject").toString();
    lesson.student1Memo = object.value("student1Memo").toString();
    lesson.student2Memo = object.value("student2Memo").toString();

    return lesson;
}

QString MainWindow::scheduleToJson() const
{
    QJsonArray daysArray;

    for (const auto &day : schedule)
    {
        QJsonArray teachersArray;

        for (const auto &teacher : day)
        {
            QJsonObject teacherObject;
            teacherObject["teacherName"] = teacher.teacherName;

            QJsonArray lessonsArray;

            for (const auto &lesson : teacher.lessons)
            {
                lessonsArray.append(QJsonDocument::fromJson(lessonToJson(lesson).toUtf8()).object());
            }

            teacherObject["lessons"] = lessonsArray;
            teachersArray.append(teacherObject);
        }

        daysArray.append(teachersArray);
    }

    QJsonDocument document(daysArray);
    QString jsonString = QString::fromUtf8(document.toJson(QJsonDocument::Compact));
    return jsonString;
}

void MainWindow::jsonToSchedule(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isArray())
    {
        statusBar()->showMessage("読み込めるスケジュールデータではありません", 2000);
        return;
    }

    schedule.clear();

    const QJsonArray daysArray = document.array();

    for (int dayIndex = 0; dayIndex < daysArray.size(); ++dayIndex)
    {
        QVector<TeacherColumn> dayColumns;
        const QJsonArray teachersArray = daysArray[dayIndex].toArray();

        for (const QJsonValue &teacherValue : teachersArray)
        {
            const QJsonObject teacherObject = teacherValue.toObject();

            TeacherColumn teacher;
            teacher.teacherName = teacherObject.value("teacherName").toString();
            teacher.lessons.resize(periods.size());

            const QJsonArray lessonsArray = teacherObject.value("lessons").toArray();

            for (int lessonIndex = 0; lessonIndex < lessonsArray.size() && lessonIndex < periods.size(); ++lessonIndex)
            {
                const QJsonObject lessonObject = lessonsArray[lessonIndex].toObject();

                CellData lesson;
                lesson.student1Name = lessonObject.value("student1Name").toString();
                lesson.student1Grade = lessonObject.value("student1Grade").toString();
                lesson.student1Subject = lessonObject.value("student1Subject").toString();
                lesson.student2Name = lessonObject.value("student2Name").toString();
                lesson.student2Grade = lessonObject.value("student2Grade").toString();
                lesson.student2Subject = lessonObject.value("student2Subject").toString();
                lesson.student1Memo = lessonObject.value("student1Memo").toString();
                lesson.student2Memo = lessonObject.value("student2Memo").toString();

                teacher.lessons[lessonIndex] = lesson;
            }

            dayColumns.append(teacher);
        }

        if (dayColumns.isEmpty())
        {
            TeacherColumn emptyColumn;
            emptyColumn.lessons.resize(periods.size());
            dayColumns.append(emptyColumn);
        }

        schedule.append(dayColumns);
    }

    while (schedule.size() < days.size())
    {
        TeacherColumn emptyColumn;
        emptyColumn.lessons.resize(periods.size());
        schedule.append(QVector<TeacherColumn>{emptyColumn});
    }

    renderTable();
}

void MainWindow::saveScheduleToFile()
{
    QDir dir(schedulesDirPath());

    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    const QString json = scheduleToJson();

    const QString datedFileName =
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss") + ".json";

    const QString datedPath = dir.filePath(datedFileName);
    const QString latestPath = dir.filePath("latest.json");

    QFile datedFile(datedPath);
    if (!datedFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "保存エラー", "時間割を保存できませんでした。");
        return;
    }

    datedFile.write(json.toUtf8());
    datedFile.close();

    QFile latestFile(latestPath);
    if (latestFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        latestFile.write(json.toUtf8());
        latestFile.close();
    }

    statusBar()->showMessage("時間割を保存しました", 2000);
}

void MainWindow::loadScheduleButton()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        "時間割を読み込み",
        schedulesDirPath(),
        "JSON (*.json)");

    if (fileName.isEmpty())
    {
        return;
    }

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "読み込みエラー", "時間割を読み込めませんでした。");
        return;
    }

    jsonToSchedule(QString::fromUtf8(file.readAll()));
    statusBar()->showMessage("時間割を読み込みました", 2000);
}

void MainWindow::loadLatestSchedule()
{
    const QString latestPath = QDir(schedulesDirPath()).filePath("latest.json");

    QFile file(latestPath);

    if (!file.exists())
    {
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    jsonToSchedule(QString::fromUtf8(file.readAll()));
    statusBar()->showMessage("最新の時間割を読み込みました", 2000);
}

QDate MainWindow::mondayOf(const QDate &date) const
{
    return date.addDays(1 - date.dayOfWeek());
}

QString MainWindow::schedulesDirPath() const
{
    return QCoreApplication::applicationDirPath() + "/schedules";
}

QString MainWindow::scheduleFilePath(const QDate &monday){
	QDir dir(schedulesDirPath());

	if(!dir.exists()){
		dir.mkpath(".");
	}

	return dir.filePath(monday.toString("yyyy-MM-dd") + ".json");
}
