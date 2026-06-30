#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QScrollBar>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QVariantList>
#include <QWheelEvent>
#include <QWidget>

namespace
{
    bool containsIndex(const QVariantList &indexes, int index)
    {
        for (const QVariant &value : indexes)
        {
            if (value.toInt() == index)
            {
                return true;
            }
        }

        return false;
    }

    bool isPeriodEndRow(const QObject *table, int row)
    {
        const int maxStudents =
            table->property("maxStudentPerTeacher").toInt();

        return maxStudents > 0 && (row + 1) % maxStudents == 0;
    }

    bool isDayEndColumn(const QObject *table, int column)
    {
        return containsIndex(
            table->property("dayEndColumns").toList(),
            column);
    }

    QColor scheduleColorProperty(
        const QObject *table,
        const char *name,
        const QColor &defaultColor)
    {
        const QColor color(table->property(name).toString());
        return color.isValid() ? color : defaultColor;
    }

    int scheduleWidthProperty(
        const QObject *table,
        const char *name,
        int defaultWidth)
    {
        const QVariant value = table->property(name);

        if (!value.isValid() || !value.canConvert<int>())
        {
            return defaultWidth;
        }

        return qMax(0, value.toInt());
    }

    void drawScheduleLine(
        QPainter *painter,
        const QColor &color,
        int width,
        const QPointF &from,
        const QPointF &to)
    {
        if (width <= 0)
        {
            return;
        }

        painter->setPen(QPen(color, width));
        painter->drawLine(from, to);
    }

    void drawScheduleLines(
        const QObject *table,
        QPainter *painter,
        const QRect &rect,
        bool drawRightThick,
        bool drawBottomThick)
    {
        painter->save();

        const QColor verticalLineColor = scheduleColorProperty(
            table,
            "scheduleVerticalLineColor",
            QColor(125, 125, 125));
        const QColor horizontalLineColor = scheduleColorProperty(
            table,
            "scheduleHorizontalLineColor",
            QColor(125, 125, 125));
        const QColor verticalSectionLineColor = scheduleColorProperty(
            table,
            "scheduleVerticalSectionLineColor",
            QColor(55, 55, 55));
        const QColor horizontalSectionLineColor = scheduleColorProperty(
            table,
            "scheduleHorizontalSectionLineColor",
            QColor(55, 55, 55));
        const int verticalLineWidth = scheduleWidthProperty(
            table,
            "scheduleVerticalLineWidth",
            1);
        const int horizontalLineWidth = scheduleWidthProperty(
            table,
            "scheduleHorizontalLineWidth",
            1);
        const int verticalSectionLineWidth = scheduleWidthProperty(
            table,
            "scheduleVerticalSectionLineWidth",
            3);
        const int horizontalSectionLineWidth = scheduleWidthProperty(
            table,
            "scheduleHorizontalSectionLineWidth",
            3);

        const qreal verticalLineOffset =
            qMax(0, verticalLineWidth - 1) / 2.0;
        const qreal horizontalLineOffset =
            qMax(0, horizontalLineWidth - 1) / 2.0;
        const qreal verticalSectionLineOffset =
            qMax(0, verticalSectionLineWidth - 1) / 2.0;
        const qreal horizontalSectionLineOffset =
            qMax(0, horizontalSectionLineWidth - 1) / 2.0;

        drawScheduleLine(
            painter,
            verticalLineColor,
            verticalLineWidth,
            QPointF(rect.right() - verticalLineOffset, rect.top()),
            QPointF(rect.right() - verticalLineOffset, rect.bottom()));
        drawScheduleLine(
            painter,
            horizontalLineColor,
            horizontalLineWidth,
            QPointF(rect.left(), rect.bottom() - horizontalLineOffset),
            QPointF(rect.right(), rect.bottom() - horizontalLineOffset));

        if (drawRightThick)
        {
            drawScheduleLine(
                painter,
                verticalSectionLineColor,
                verticalSectionLineWidth,
                QPointF(rect.right() - verticalSectionLineOffset, rect.top()),
                QPointF(rect.right() - verticalSectionLineOffset, rect.bottom()));
        }

        if (drawBottomThick)
        {
            drawScheduleLine(
                painter,
                horizontalSectionLineColor,
                horizontalSectionLineWidth,
                QPointF(rect.left(), rect.bottom() - horizontalSectionLineOffset),
                QPointF(rect.right(), rect.bottom() - horizontalSectionLineOffset));
        }

        painter->restore();
    }

    class ScheduleCellDelegate : public QStyledItemDelegate
    {
    public:
        explicit ScheduleCellDelegate(QObject *parent = nullptr)
            : QStyledItemDelegate(parent)
        {
        }

        void paint(
            QPainter *painter,
            const QStyleOptionViewItem &option,
            const QModelIndex &index) const override
        {
            const QObject *table = parent();
            if (table == nullptr)
            {
                QStyledItemDelegate::paint(painter, option, index);
                return;
            }

            QStyleOptionViewItem itemOption(option);
            initStyleOption(&itemOption, index);

            const bool oddDisplayRow = (index.row() + 1) % 2 == 1;
            const bool selected = itemOption.state & QStyle::State_Selected;
            const QColor textColor = scheduleColorProperty(
                table,
                oddDisplayRow
                    ? "scheduleOddRowTextColor"
                    : "scheduleTextColor",
                QColor(0, 0, 0));
            itemOption.palette.setColor(QPalette::Text, textColor);

            if (oddDisplayRow && !selected)
            {
                itemOption.backgroundBrush = QBrush(
                    scheduleColorProperty(
                        table,
                        "scheduleOddRowColor",
                        QColor(244, 244, 244)));
            }

            const QWidget *widget = itemOption.widget;
            QStyle *style = widget != nullptr
                                ? widget->style()
                                : QApplication::style();
            style->drawControl(
                QStyle::CE_ItemViewItem,
                &itemOption,
                painter,
                widget);

            drawScheduleLines(
                table,
                painter,
                option.rect,
                isDayEndColumn(table, index.column()),
                isPeriodEndRow(table, index.row()));
        }
    };

    class ScheduleHeaderView : public QHeaderView
    {
    public:
        explicit ScheduleHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr)
            : QHeaderView(orientation, parent)
        {
        }

    protected:
        void paintSection(
            QPainter *painter,
            const QRect &rect,
            int logicalIndex) const override
        {
            QHeaderView::paintSection(painter, rect, logicalIndex);

            const QObject *table = parent();
            if (table == nullptr)
            {
                return;
            }

            const bool horizontal = orientation() == Qt::Horizontal;
            drawScheduleLines(
                table,
                painter,
                rect,
                horizontal && isDayEndColumn(table, logicalIndex),
                horizontal || isPeriodEndRow(table, logicalIndex));
        }
    };
}

void MainWindow::setupScheduleTab()
{
    ui->scheduleTable->setItemDelegate(
        new ScheduleCellDelegate(ui->scheduleTable));
    ui->scheduleTable->setHorizontalHeader(
        new ScheduleHeaderView(Qt::Horizontal, ui->scheduleTable));
    ui->scheduleTable->setVerticalHeader(
        new ScheduleHeaderView(Qt::Vertical, ui->scheduleTable));

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
    connect(ui->copyCellButton, &QPushButton::clicked, this, &MainWindow::copyCell);
    connect(ui->pasteCellButton, &QPushButton::clicked, this, &MainWindow::pasteCell);
    connect(ui->cutCellButton, &QPushButton::clicked, this, &MainWindow::cutCell);

    connect(ui->saveScheduleButton, &QPushButton::clicked, this, &MainWindow::saveScheduleToFile);
    connect(ui->loadScheduleButton, &QPushButton::clicked, this, &MainWindow::loadScheduleButton);

    connect(ui->lastWeekButton, &QPushButton::clicked, this, &MainWindow::showLastWeek);
    connect(ui->thisWeekButton, &QPushButton::clicked, this, &MainWindow::showThisWeek);
    connect(ui->nextWeekButton, &QPushButton::clicked, this, &MainWindow::showNextWeek);
    connect(ui->copyToThisWeek, &QPushButton::clicked, this, &MainWindow::copyCurrentWeekToThisWeek);

    connect(
        ui->student1GradeComboBox,
        &QComboBox::currentTextChanged,
        this,
        [this](const QString &grade)
        {
            updateStudentComboBox(ui->student1ComboBox, grade);
        });

    connect(
        ui->student1SubjectComboBox,
        &QComboBox::currentTextChanged,
        this,
        [this](const QString &)
        {
            updateCell();
        });

    connect(ui->undoButton, &QPushButton::clicked, this, &MainWindow::undoCellEdit);
    connect(ui->redoButton, &QPushButton::clicked, this, &MainWindow::redoCellEdit);

    ui->scheduleTable->viewport()->installEventFilter(this);
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

    QVariantList dayEndColumns;
    int firstColumn = 0;

    for (const QVector<TeacherColumn> &daySchedule : schedule)
    {
        firstColumn += daySchedule.size();

        if (firstColumn > 0)
        {
            dayEndColumns.append(firstColumn - 1);
        }
    }

    ui->scheduleTable->setProperty("dayEndColumns", dayEndColumns);
    ui->scheduleTable->setProperty("maxStudentPerTeacher", MaxStudentPerTeacher);
    ui->scheduleTable->setProperty("scheduleOddRowColor", scheduleOddRowColor);
    ui->scheduleTable->setProperty("scheduleTextColor", scheduleTextColor);
    ui->scheduleTable->setProperty("scheduleOddRowTextColor", scheduleOddRowTextColor);
    ui->scheduleTable->setProperty("scheduleVerticalLineColor", scheduleVerticalLineColor);
    ui->scheduleTable->setProperty("scheduleVerticalLineWidth", scheduleVerticalLineWidth);
    ui->scheduleTable->setProperty("scheduleHorizontalLineColor", scheduleHorizontalLineColor);
    ui->scheduleTable->setProperty("scheduleHorizontalLineWidth", scheduleHorizontalLineWidth);
    ui->scheduleTable->setProperty(
        "scheduleVerticalSectionLineColor",
        scheduleVerticalSectionLineColor);
    ui->scheduleTable->setProperty(
        "scheduleVerticalSectionLineWidth",
        scheduleVerticalSectionLineWidth);
    ui->scheduleTable->setProperty(
        "scheduleHorizontalSectionLineColor",
        scheduleHorizontalSectionLineColor);
    ui->scheduleTable->setProperty(
        "scheduleHorizontalSectionLineWidth",
        scheduleHorizontalSectionLineWidth);

    ui->scheduleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->scheduleTable->horizontalHeader()->setDefaultSectionSize(cellSectionSize);

    // 行数を増やしても、表全体の高さは従来どおり画面に合わせる。
    ui->scheduleTable->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->scheduleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->scheduleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->scheduleTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    ui->scheduleTable->setWordWrap(true);
    ui->scheduleTable->setShowGrid(false);
    ui->scheduleTable->setStyleSheet(
        "QHeaderView::section {"
        "  border: none;"
        "  padding: 4px;"
        "}"
        "QTableWidget {"
        "  background: white;"
        "}");

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
        selectedRow = 0;
        selectedColumn = 0;
        renderEntry();
    }
}

void MainWindow::addTeacherColumn()
{
    const int oldRow = selectedRow;
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
    clearCellEditHistory();

    const int newColumnIndex = firstColumnOfDay(dayIndex) + schedule[dayIndex].size() - 1;

    loadCell(oldRow, newColumnIndex);
}

void MainWindow::removeTeacherColumn()
{
    const int oldRow = selectedRow;
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
        clearCellEditHistory();

        const int column = firstColumnOfDay(dayIndex);
        selectedRow = -1;
        selectedColumn = -1;
        loadCell(0, column);
        return;
    }

    schedule[dayIndex].removeAt(teacherIndex);

    renderTable();
    clearCellEditHistory();

    const int newSelectedColumn = firstColumnOfDay(dayIndex);
    selectedRow = -1;
    selectedColumn = -1;
    loadCell(oldRow, newSelectedColumn);
}

void MainWindow::renameTeacherColumn()
{
    updateCell();

    const int oldRow = selectedRow;
    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);

    if (dayIndex < 0 || teacherIndex < 0)
    {
        return;
    }

    schedule[dayIndex][teacherIndex].teacherName =
        ui->teacherComboBox->currentText().trimmed();

    const int newColumn = firstColumnOfDay(dayIndex) + teacherIndex;

    renderTable();
    clearCellEditHistory();

    loadCell(oldRow, newColumn);
}

bool MainWindow::isValidCellIndex(int row, int column)
{
    if (row < 0 || row >= tableRowCount() ||
        column < 0 || column >= ui->scheduleTable->columnCount())
    {
        return false;
    }

    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);
    const int periodIndex = periodIndexFromTableRow(row);
    const int studentIndex = studentIndexFromTableRow(row);

    if (dayIndex < 0 || dayIndex >= schedule.size() ||
        teacherIndex < 0 || teacherIndex >= schedule[dayIndex].size() ||
        periodIndex < 0 || periodIndex >= periods.size() ||
        studentIndex < 0 || studentIndex >= MaxStudentPerTeacher)
    {
        return false;
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return false;
    }

    return true;
}

void MainWindow::loadCell(int row, int column)
{
    ui->scheduleTable->setCurrentCell(row, column);

    if (!isValidCellIndex(row, column))
    {
        return;
    }

    selectedRow = row;
    selectedColumn = column;

    isLoadingCell = true;

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

    isLoadingCell = false;
}

void MainWindow::updateCell()
{
    if (isLoadingCell)
    {
        return;
    }

    if (!isValidCellIndex(selectedRow, selectedColumn))
    {
        return;
    }

    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

    const LessonData before =
        schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex];

    LessonData after;
    after.studentName = ui->student1ComboBox->currentText();
    after.studentGrade = ui->student1GradeComboBox->currentText();
    after.subject = ui->student1SubjectComboBox->currentText();
    after.memo = ui->student1MemoTextEdit->toPlainText();

    if (lessonDataEquals(before, after))
    {
        return;
    }

    schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex] = after;

    pushCellEdit(selectedRow, selectedColumn, before, after);

    renderCell(selectedRow, selectedColumn);
}

void MainWindow::renderCell(int row, int column)
{
    if (!isValidCellIndex(row, column))
    {
        return;
    }

    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);
    const int periodIndex = periodIndexFromTableRow(row);
    const int studentIndex = studentIndexFromTableRow(row);

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
    if (!isValidCellIndex(selectedRow, selectedColumn))
    {
        return;
    }

    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

    const LessonData before =
        schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex];

    const LessonData after;

    if (lessonDataEquals(before, after))
    {
        return;
    }

    schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex] = after;

    pushCellEdit(selectedRow, selectedColumn, before, after);

    renderEntry();
    renderCell(selectedRow, selectedColumn);
}

void MainWindow::renderEntry()
{
    if (!isValidCellIndex(selectedRow, selectedColumn))
    {
        return;
    }

    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

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
    statusBar()->showMessage("コピーしました", 2000);
}

void MainWindow::pasteCell()
{
    if (!isValidCellIndex(selectedRow, selectedColumn))
    {
        return;
    }

    const int dayIndex = dayIndexFromColumn(selectedColumn);
    const int teacherIndex = teacherIndexFromColumn(selectedColumn);
    const int periodIndex = periodIndexFromTableRow(selectedRow);
    const int studentIndex = studentIndexFromTableRow(selectedRow);

    const QString json = QApplication::clipboard()->text();
    if (json.trimmed().isEmpty())
    {
        statusBar()->showMessage("貼り付けるセルデータがありません", 2000);
        return;
    }

    const LessonData before =
        schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex];

    const LessonData after = jsonToLesson(json);

    if (lessonDataEquals(before, after))
    {
        return;
    }

    schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex] = after;

    pushCellEdit(selectedRow, selectedColumn, before, after);

    int oldselectedRow = selectedRow;
    int oldselectedColumn = selectedColumn;
    selectedRow = -1;
    selectedColumn = -1;

    renderCell(oldselectedRow, oldselectedColumn);
    loadCell(oldselectedRow, oldselectedColumn);

    statusBar()->showMessage("貼り付けました", 2000);
}

void MainWindow::cutCell()
{
    copyCell();
    clearCell();
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

void MainWindow::copyCurrentWeekToThisWeek()
{
    updateCell();

    const QDate thisMonday = mondayOf(QDate::currentDate());

    if (!scheduleMonday.isValid())
    {
        QMessageBox::warning(this, "コピーできません", "現在の週が設定されていません。");
        return;
    }

    if (scheduleMonday == thisMonday)
    {
        statusBar()->showMessage("すでに今週の時間割です", 2000);
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        "今週にコピー",
        QString("%1 の週の時間割を、今週 %2 の週にコピーします。")
            .arg(scheduleMonday.toString("yyyy年M月d日"))
            .arg(thisMonday.toString("yyyy年M月d日")),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes)
    {
        return;
    }

    scheduleMonday = thisMonday;
    saveScheduleToFile();
    renderTable();
    clearCellEditHistory();

    statusBar()->showMessage("この週を今週にコピーしました", 2000);
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
