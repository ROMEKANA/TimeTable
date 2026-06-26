#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::setupTable()
{
    scheduleTabConnects();
    loadLatestSchedule();
}

void MainWindow::scheduleTabConnects()
{
    connect(
        ui->scheduleTable,
        &QTableWidget::currentCellChanged,
        this,
        [this](int row, int column)
        {
            loadCell(row, column);
        });

    connect(ui->applyCellButton, &QPushButton::clicked, this, &MainWindow::updateCell);
    connect(ui->clearCellButton, &QPushButton::clicked, this, &MainWindow::clearCell);
    connect(ui->addTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::addTeacherColumn);
    connect(ui->removeTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::removeTeacherColumn);
    connect(ui->renameTeacherColumnButton, &QPushButton::clicked, this, &MainWindow::renameTeacherColumn);
    connect(ui->copyButton, &QPushButton::clicked, this, &MainWindow::copyCell);
    connect(ui->pasteButton, &QPushButton::clicked, this, &MainWindow::pasteCell);

    connect(ui->saveScheduleButton, &QPushButton::clicked, this, &MainWindow::saveScheduleToFile);
    connect(ui->loadScheduleButton, &QPushButton::clicked, this, &MainWindow::loadScheduleButton);

    connect(ui->lastWeekButton, &QPushButton::clicked, this, &MainWindow::showLastWeek);
    connect(ui->thisWeekButton, &QPushButton::clicked, this, &MainWindow::showThisWeek);
    connect(ui->nextWeekButton, &QPushButton::clicked, this, &MainWindow::showNextWeek);

    connect(
        ui->student1GradeComboBox,
        &QComboBox::currentTextChanged,
        this,
        [this](const QString &grade)
        {
            updateStudentComboBox(ui->student1ComboBox, grade);
        });

    ui->scheduleTable->viewport()->installEventFilter(this);
}

void MainWindow::initializeTeacherLessons(TeacherColumn &teacher)
{
    teacher.lessons.clear();
    teacher.lessons.resize(periods.size());

    for (QVector<LessonData> &periodLessons : teacher.lessons)
    {
        periodLessons.resize(MaxStudentPerTeacher);
    }
}

void MainWindow::initializeTable()
{
    schedule.clear();

    for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex)
    {
        TeacherColumn emptyColumn;
        initializeTeacherLessons(emptyColumn);
        schedule.append(QVector<TeacherColumn>{emptyColumn});
    }
}

int MainWindow::tableRowCount() const
{
    return periods.size() * MaxStudentPerTeacher;
}

int MainWindow::periodIndexFromTableRow(int tableRow) const
{
    if (tableRow < 0 || MaxStudentPerTeacher <= 0)
    {
        return -1;
    }

    return tableRow / MaxStudentPerTeacher;
}

int MainWindow::studentIndexFromTableRow(int tableRow) const
{
    if (tableRow < 0 || MaxStudentPerTeacher <= 0)
    {
        return -1;
    }

    return tableRow % MaxStudentPerTeacher;
}

int MainWindow::tableRowOf(int periodIndex, int studentIndex) const
{
    if (periodIndex < 0 || periodIndex >= periods.size() ||
        studentIndex < 0 || studentIndex >= MaxStudentPerTeacher)
    {
        return -1;
    }

    return periodIndex * MaxStudentPerTeacher + studentIndex;
}

void MainWindow::renderTable()
{
    // 再構築中の currentCellChanged で、別の時間割へ古い編集内容を
    // 書き戻さないようにする。
    selectedRow = -1;
    selectedColumn = -1;

    QStringList horizontalHeaders;
    QStringList verticalHeaders;
    int totalColumns = 0;

    for (int dayIndex = 0; dayIndex < schedule.size(); ++dayIndex)
    {
        for (int teacherIndex = 0; teacherIndex < schedule[dayIndex].size(); ++teacherIndex)
        {
            QString dayText;

            if (teacherIndex == 0)
            {
                const QDate date = scheduleMonday.addDays(dayIndex);
                dayText = QString("%1\t%2")
                              .arg(date.toString("M/d"))
                              .arg(days.value(dayIndex));
            }

            const QString teacherName =
                schedule[dayIndex][teacherIndex].teacherName.trimmed();

            horizontalHeaders << QString("%1\n%2")
                                     .arg(dayText, teacherName.isEmpty() ? "講師未設定" : teacherName);

            ++totalColumns;
        }
    }

    for (const QString &period : periods)
    {
        verticalHeaders << period;

        for (int studentIndex = 1; studentIndex < MaxStudentPerTeacher; ++studentIndex)
        {
            verticalHeaders << "";
        }
    }

    ui->scheduleTable->clear();
    ui->scheduleTable->setColumnCount(totalColumns);
    ui->scheduleTable->setRowCount(tableRowCount());
    ui->scheduleTable->setHorizontalHeaderLabels(horizontalHeaders);
    ui->scheduleTable->setVerticalHeaderLabels(verticalHeaders);

    ui->scheduleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->scheduleTable->horizontalHeader()->setDefaultSectionSize(cellDefaultSectionSize);

    // 行数を増やしても、表全体の高さは従来どおり画面に合わせる。
    ui->scheduleTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->scheduleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->scheduleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->scheduleTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->scheduleTable->setWordWrap(true);

    for (int row = 0; row < tableRowCount(); ++row)
    {
        for (int column = 0; column < totalColumns; ++column)
        {
            auto *item = new QTableWidgetItem;
            item->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);
            ui->scheduleTable->setItem(row, column, item);
            renderCell(row, column);
        }
    }

    if (totalColumns > 0 && tableRowCount() > 0)
    {
        ui->scheduleTable->setCurrentCell(0, 0);
        loadCell(0, 0);
    }
}

int MainWindow::firstColumnOfDay(int dayIndex) const
{
    int column = 0;

    for (int i = 0; i < dayIndex && i < schedule.size(); ++i)
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
    updateCell();

    int dayIndex = dayIndexFromColumn(selectedColumn);

    if (dayIndex < 0 || dayIndex >= schedule.size())
    {
        dayIndex = 0;
    }

    if (dayIndex < 0 || dayIndex >= schedule.size())
    {
        return;
    }

    TeacherColumn newColumn;
    initializeTeacherLessons(newColumn);

    schedule[dayIndex].append(newColumn);

    renderTable();

    const int newColumnIndex =
        firstColumnOfDay(dayIndex) + schedule[dayIndex].size() - 1;

    ui->scheduleTable->setCurrentCell(0, newColumnIndex);
    loadCell(0, newColumnIndex);
}

void MainWindow::removeTeacherColumn()
{
    updateCell();

    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    if (schedule[dayIndex].size() <= 1)
    {
        TeacherColumn &teacher = schedule[dayIndex][0];
        teacher.teacherName.clear();
        initializeTeacherLessons(teacher);

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
    updateCell();

    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    schedule[dayIndex][teacherIndex].teacherName =
        ui->teacherComboBox->currentText().trimmed();

    renderTable();

    const int column = firstColumnOfDay(dayIndex) + teacherIndex;
    ui->scheduleTable->setCurrentCell(0, column);
    loadCell(0, column);
}

void MainWindow::loadCell(int row, int column)
{
    updateCell();

    selectedRow = row;
    selectedColumn = column;

    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);
    const int periodIndex = periodIndexFromTableRow(row);
    const int studentIndex = studentIndexFromTableRow(row);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || periodIndex >= periods.size() ||
        studentIndex < 0 || studentIndex >= MaxStudentPerTeacher)
    {
        return;
    }

    ui->student1GradeComboBox->clear();
    ui->student1GradeComboBox->addItem("");

    for (const GradeStudents &gradeStudents : allStudents)
    {
        if (!gradeStudents.students.isEmpty())
        {
            ui->student1GradeComboBox->addItem(gradeStudents.Grade);
        }
    }

    updateTeacherComboBox(ui->teacherComboBox);

    ui->student1SubjectComboBox->clear();
    ui->student1SubjectComboBox->addItem("");
    ui->student1SubjectComboBox->addItems(subjects);

    updateStudentComboBox(
        ui->student1ComboBox,
        ui->student1GradeComboBox->currentText());

    renderEntry();
}

void MainWindow::updateCell()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || periodIndex >= periods.size() ||
        studentIndex < 0 || studentIndex >= MaxStudentPerTeacher)
    {
        return;
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return;
    }

    LessonData lesson;
    lesson.studentName = ui->student1ComboBox->currentText();
    lesson.studentGrade = ui->student1GradeComboBox->currentText();
    lesson.subject = ui->student1SubjectComboBox->currentText();
    lesson.memo = ui->student1MemoTextEdit->toPlainText();

    schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex] = lesson;

    renderCell(selectedRow, selectedColumn);
}

QString MainWindow::cellTextFromData(const LessonData &lesson) const
{
    QStringList lines;

    const QString info = QString("%1 %2")
                             .arg(lesson.studentGrade.trimmed(), lesson.subject.trimmed())
                             .trimmed();

    if (!info.isEmpty())
    {
        lines << info;
    }

    if (!lesson.studentName.trimmed().isEmpty())
    {
        lines << lesson.studentName.trimmed();
    }

    return lines.join('\n');
}

void MainWindow::renderCell(int row, int column)
{
    if (row < 0 || row >= tableRowCount() ||
        column < 0 || column >= ui->scheduleTable->columnCount())
    {
        return;
    }

    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);
    const int periodIndex = periodIndexFromTableRow(row);
    const int studentIndex = studentIndexFromTableRow(row);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || periodIndex >= periods.size() ||
        studentIndex < 0 || studentIndex >= MaxStudentPerTeacher)
    {
        return;
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return;
    }

    auto *item = ui->scheduleTable->item(row, column);
    if (item != nullptr)
    {
        item->setText(
            cellTextFromData(
                schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex]));
    }
}

void MainWindow::clearCell()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || studentIndex < 0)
    {
        return;
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return;
    }

    schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex] =
        LessonData();

    renderEntry();
    renderCell(selectedRow, selectedColumn);
}

void MainWindow::renderEntry()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || studentIndex < 0)
    {
        return;
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return;
    }

    const TeacherColumn &teacher = schedule[dayIndex][teacherIndex];
    const LessonData &lesson = teacher.lessons[periodIndex][studentIndex];

    ui->teacherComboBox->setCurrentText(teacher.teacherName);
    ui->student1GradeComboBox->setCurrentText(lesson.studentGrade);
    updateStudentComboBox(ui->student1ComboBox, lesson.studentGrade);
    ui->student1ComboBox->setCurrentText(lesson.studentName);
    ui->student1SubjectComboBox->setCurrentText(lesson.subject);
    ui->student1MemoTextEdit->setPlainText(lesson.memo);
}

void MainWindow::copyCell()
{
    const QString json = lessonToJson(selectedRow, selectedColumn);

    if (json.isEmpty())
    {
        return;
    }

    QApplication::clipboard()->setText(json);
    statusBar()->showMessage("生徒1人分をコピーしました", 2000);
}

void MainWindow::pasteCell()
{
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || studentIndex < 0)
    {
        return;
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return;
    }

    const QString json = QApplication::clipboard()->text();
    if (json.trimmed().isEmpty())
    {
        statusBar()->showMessage("貼り付けるセルデータがありません", 2000);
        return;
    }

    schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex] =
        jsonToLesson(json);

    renderCell(selectedRow, selectedColumn);
    loadCell(selectedRow, selectedColumn);

    statusBar()->showMessage("生徒1人分を貼り付けました", 2000);
}

bool MainWindow::lessonDataIsEmpty(const LessonData &lesson) const
{
    return lesson.studentName.trimmed().isEmpty() &&
           lesson.studentGrade.trimmed().isEmpty() &&
           lesson.subject.trimmed().isEmpty() &&
           lesson.memo.trimmed().isEmpty();
}

void MainWindow::updateStudentComboBox(QComboBox *comboBox, const QString &grade)
{
    const QString currentName = comboBox->currentText();

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

    const int index = comboBox->findText(currentName);
    if (index >= 0)
    {
        comboBox->setCurrentIndex(index);
    }
}

void MainWindow::updateTeacherComboBox(QComboBox *comboBox)
{
    const QString currentName = comboBox->currentText();

    comboBox->clear();
    comboBox->addItem("");

    for (const TeacherData &teacher : teachers)
    {
        comboBox->addItem(teacher.name);
    }

    const int index = comboBox->findText(currentName);
    if (index >= 0)
    {
        comboBox->setCurrentIndex(index);
    }
}

QString MainWindow::lessonToJson(const LessonData &lesson) const
{
    QJsonObject object;
    object["studentName"] = lesson.studentName;
    object["studentGrade"] = lesson.studentGrade;
    object["subject"] = lesson.subject;
    object["memo"] = lesson.memo;

    return QString::fromUtf8(
        QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString MainWindow::lessonToJson(int row, int column) const
{
    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);
    const int periodIndex = periodIndexFromTableRow(row);
    const int studentIndex = studentIndexFromTableRow(row);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || studentIndex < 0)
    {
        return QString();
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return QString();
    }

    return lessonToJson(
        schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex]);
}

LessonData MainWindow::jsonToLesson(const QString &json) const
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject())
    {
        statusBar()->showMessage("貼り付けできるセルデータではありません", 2000);
        return LessonData();
    }

    const QJsonObject object = document.object();
    LessonData lesson;

    if (object.contains("studentName"))
    {
        lesson.studentName = object.value("studentName").toString();
        lesson.studentGrade = object.value("studentGrade").toString();
        lesson.subject = object.value("subject").toString();
        lesson.memo = object.value("memo").toString();
        return lesson;
    }

    // 旧形式のコピー内容は、生徒1として貼り付ける。
    lesson.studentName = object.value("student1Name").toString();
    lesson.studentGrade = object.value("student1Grade").toString();
    lesson.subject = object.value("student1Subject").toString();
    lesson.memo = object.value("student1Memo").toString();

    return lesson;
}

QString MainWindow::scheduleToJson() const
{
    QJsonArray daysArray;

    for (const QVector<TeacherColumn> &day : schedule)
    {
        QJsonArray teachersArray;

        for (const TeacherColumn &teacher : day)
        {
            QJsonObject teacherObject;
            teacherObject["teacherName"] = teacher.teacherName;

            QJsonArray periodsArray;

            for (const QVector<LessonData> &periodLessons : teacher.lessons)
            {
                QJsonArray studentArray;

                for (const LessonData &lesson : periodLessons)
                {
                    const QJsonDocument lessonDocument =
                        QJsonDocument::fromJson(lessonToJson(lesson).toUtf8());

                    studentArray.append(lessonDocument.object());
                }

                periodsArray.append(studentArray);
            }

            teacherObject["lessons"] = periodsArray;
            teachersArray.append(teacherObject);
        }

        daysArray.append(teachersArray);
    }

    QJsonObject root;
    root["version"] = 2;
    root["monday"] = scheduleMonday.toString("yyyy-MM-dd");
    root["schedule"] = daysArray;

    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool MainWindow::jsonToSchedule(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject())
    {
        return false;
    }

    const QJsonObject root = document.object();

    if (!root.contains("monday") || !root.contains("schedule"))
    {
        return false;
    }

    const QDate fileMonday = QDate::fromString(
        root.value("monday").toString(),
        "yyyy-MM-dd");

    if (!fileMonday.isValid())
    {
        return false;
    }

    const QJsonArray daysArray = root.value("schedule").toArray();

    schedule.clear();
    scheduleMonday = fileMonday;

    for (int dayIndex = 0; dayIndex < days.size(); ++dayIndex)
    {
        QVector<TeacherColumn> dayColumns;
        const QJsonArray teachersArray =
            dayIndex < daysArray.size()
                ? daysArray.at(dayIndex).toArray()
                : QJsonArray();

        for (const QJsonValue &teacherValue : teachersArray)
        {
            const QJsonObject teacherObject = teacherValue.toObject();
            TeacherColumn teacher;
            teacher.teacherName = teacherObject.value("teacherName").toString();
            initializeTeacherLessons(teacher);

            const QJsonArray lessonsArray = teacherObject.value("lessons").toArray();

            for (int periodIndex = 0;
                 periodIndex < lessonsArray.size() &&
                 periodIndex < teacher.lessons.size();
                 ++periodIndex)
            {
                const QJsonValue periodValue = lessonsArray.at(periodIndex);

                if (periodValue.isArray())
                {
                    const QJsonArray studentArray = periodValue.toArray();

                    for (int studentIndex = 0;
                         studentIndex < studentArray.size() &&
                         studentIndex < teacher.lessons[periodIndex].size();
                         ++studentIndex)
                    {
                        const QJsonObject lessonObject =
                            studentArray.at(studentIndex).toObject();

                        teacher.lessons[periodIndex][studentIndex] =
                            jsonToLesson(QString::fromUtf8(
                                QJsonDocument(lessonObject).toJson(QJsonDocument::Compact)));
                    }

                    continue;
                }

                // 旧形式: 1時限のオブジェクトに生徒1・2が入っていた。
                const QJsonObject oldLesson = periodValue.toObject();

                if (!teacher.lessons[periodIndex].isEmpty())
                {
                    teacher.lessons[periodIndex][0] =
                        jsonToLesson(QString::fromUtf8(
                            QJsonDocument(oldLesson).toJson(
                                QJsonDocument::Compact)));
                }

                if (teacher.lessons[periodIndex].size() >= 2)
                {
                    LessonData secondLesson;
                    secondLesson.studentName =
                        oldLesson.value("student2Name").toString();
                    secondLesson.studentGrade =
                        oldLesson.value("student2Grade").toString();
                    secondLesson.subject =
                        oldLesson.value("student2Subject").toString();
                    secondLesson.memo =
                        oldLesson.value("student2Memo").toString();

                    teacher.lessons[periodIndex][1] = secondLesson;
                }
            }

            dayColumns.append(teacher);
        }

        if (dayColumns.isEmpty())
        {
            TeacherColumn emptyColumn;
            initializeTeacherLessons(emptyColumn);
            dayColumns.append(emptyColumn);
        }

        schedule.append(dayColumns);
    }

    renderTable();
    return true;
}

void MainWindow::saveScheduleToFile()
{
    updateCell();

    if (!scheduleMonday.isValid())
    {
        QMessageBox::warning(
            this,
            "保存エラー",
            "保存する週が設定されていません。");
        return;
    }

    QFile file(scheduleFilePath(scheduleMonday));

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(
            this,
            "保存エラー",
            "時間割を保存できませんでした。");
        return;
    }

    file.write(scheduleToJson().toUtf8());
    file.close();

    statusBar()->showMessage(
        scheduleMonday.toString("yyyy年M月d日") + "の週を保存しました",
        2000);
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

    if (!jsonToSchedule(QString::fromUtf8(file.readAll())))
    {
        QMessageBox::warning(this, "読み込みエラー", "時間割ファイルの形式が正しくありません。");
        return;
    }

    statusBar()->showMessage("時間割を読み込みました", 2000);
}

void MainWindow::loadLatestSchedule()
{
    scheduleMonday = mondayOf(QDate::currentDate());

    if (!loadScheduleFromFile(scheduleMonday))
    {
        initializeTable();
        renderTable();

        statusBar()->showMessage(
            scheduleMonday.toString("yyyy年M月d日") + "の週を新規作成しました",
            2000);
        return;
    }

    statusBar()->showMessage(
        scheduleMonday.toString("yyyy年M月d日") + "の週を読み込みました",
        2000);
}

bool MainWindow::loadScheduleFromFile(const QDate &monday)
{
    const QDate targetMonday = mondayOf(monday);
    QFile file(scheduleFilePath(targetMonday));

    if (!file.exists())
    {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "時間割を読み込めませんでした。");
        return false;
    }

    const QString json = QString::fromUtf8(file.readAll());
    file.close();

    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

    if (!document.isObject())
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "時間割ファイルの形式が正しくありません。");
        return false;
    }

    const QDate fileMonday = QDate::fromString(
        document.object().value("monday").toString(),
        "yyyy-MM-dd");

    if (fileMonday != targetMonday)
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "ファイル名と時間割の月曜日が一致しません。");
        return false;
    }

    return jsonToSchedule(json);
}

void MainWindow::switchScheduleWeek(const QDate &date)
{
    const QDate targetMonday = mondayOf(date);

    if (!targetMonday.isValid() || targetMonday == scheduleMonday)
    {
        return;
    }

    saveScheduleToFile();

    scheduleMonday = targetMonday;

    if (!loadScheduleFromFile(scheduleMonday))
    {
        initializeTable();
        renderTable();

        statusBar()->showMessage(
            scheduleMonday.toString("yyyy年M月d日") + "の週を新規作成しました",
            2000);
        return;
    }

    statusBar()->showMessage(
        scheduleMonday.toString("yyyy年M月d日") + "の週を読み込みました",
        2000);
}

void MainWindow::showLastWeek()
{
    switchScheduleWeek(scheduleMonday.addDays(-7));
}

void MainWindow::showThisWeek()
{
    switchScheduleWeek(QDate::currentDate());
}

void MainWindow::showNextWeek()
{
    switchScheduleWeek(scheduleMonday.addDays(7));
}

QDate MainWindow::mondayOf(const QDate &date) const
{
    return date.addDays(1 - date.dayOfWeek());
}

QString MainWindow::schedulesDirPath() const
{
    return QCoreApplication::applicationDirPath() + "/schedules";
}

QString MainWindow::scheduleFilePath(const QDate &monday)
{
    QDir dir(schedulesDirPath());

    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    return dir.filePath(monday.toString("yyyy-MM-dd") + ".json");
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    if (object == ui->scheduleTable->viewport() &&
        event->type() == QEvent::Wheel)
    {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        const int delta = wheelEvent->angleDelta().y();

        if (delta != 0)
        {
            QScrollBar *horizontalBar =
                ui->scheduleTable->horizontalScrollBar();

            horizontalBar->setValue(
                horizontalBar->value() - delta * scrollSpeed);

            return true;
        }
    }

    return QMainWindow::eventFilter(object, event);
}
