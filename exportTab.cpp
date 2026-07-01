#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QPaintDevice>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QStatusBar>
#include <QStringList>

#include <algorithm>

namespace
{
    struct SalaryDaySummary
    {
        int oneOnTwoCount = 0;
        int oneOnOneCount = 0;
        int highSchoolAllowanceCount = 0;
        int businessPay = 0;
        int transportPay = 0;

        int total(
            int oneOnTwoRate,
            int oneOnOneRate,
            int highSchoolAllowance) const
        {
            return oneOnTwoCount * oneOnTwoRate +
                   oneOnOneCount * oneOnOneRate +
                   highSchoolAllowanceCount * highSchoolAllowance +
                   businessPay +
                   transportPay;
        }
    };

    QString moneyText(int amount)
    {
        return QLocale(QLocale::Japanese, QLocale::Japan).toString(amount);
    }

    QString dayNameText(const QString &day)
    {
        const QString trimmed = day.trimmed();

        if (trimmed.isEmpty() || trimmed.endsWith("曜日"))
        {
            return trimmed;
        }

        return trimmed + "曜日";
    }

    bool lessonRecordLess(const LessonRecord &a, const LessonRecord &b)
    {
        if (a.date != b.date)
        {
            return a.date < b.date;
        }

        if (a.periodIndex != b.periodIndex)
        {
            return a.periodIndex < b.periodIndex;
        }

        if (a.teacherName != b.teacherName)
        {
            return a.teacherName < b.teacherName;
        }

        return a.studentName < b.studentName;
    }
}

void MainWindow::setupExportTab()
{
    connect(ui->printButton, &QPushButton::clicked, this, &MainWindow::showSchedulePrintPreview);
    connect(ui->printButton_2, &QPushButton::clicked, this, &MainWindow::showTeacherDailyPrintPreview);
    connect(ui->outputStudentSchedule, &QPushButton::clicked, this, &MainWindow::copyStudentScheduleToClipboard);
    connect(ui->salaryStatementButton, &QPushButton::clicked, this, &MainWindow::showSalaryStatementPrintPreview);
    connect(ui->guidanceReportButton, &QPushButton::clicked, this, &MainWindow::showGuidanceReportPrintPreview);
}

void MainWindow::showSchedulePrintPreview()
{
    saveScheduleToFile();

    if (schedule.isEmpty() || periods.isEmpty())
    {
        QMessageBox::warning(
            this,
            "印刷できません",
            "時間割データがありません。");
        return;
    }

    QPrinter printer(QPrinter::HighResolution);

    QPageLayout layout(
        QPageSize(QPageSize::A4),
        QPageLayout::Landscape,
        QMarginsF(8, 8, 8, 8));

    printer.setPageLayout(layout);
    printer.setDocName("時間割表");

    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle("時間割表 - 印刷プレビュー");

    connect(
        &preview,
        &QPrintPreviewDialog::paintRequested,
        this,
        &MainWindow::renderScheduleForPrint);

    preview.exec();
}

void MainWindow::showTeacherDailyPrintPreview()
{
    updateCell();

    QStringList teacherNames;

    for (const TeacherData &teacher : teachers)
    {
        const QString name = teacher.name.trimmed();

        if (!name.isEmpty() && !teacherNames.contains(name))
        {
            teacherNames.append(name);
        }
    }

    if (teacherNames.isEmpty())
    {
        QMessageBox::information(this, "講師向け印刷", "講師が登録されていません。");
        return;
    }

    const QString currentTeacher = ui->teacherComboBox->currentText().trimmed();
    int currentIndex = qMax(0, teacherNames.indexOf(currentTeacher));

    bool ok = false;
    const QString teacherName = QInputDialog::getItem(
                                    this,
                                    "講師向け印刷",
                                    "講師を選択してください",
                                    teacherNames,
                                    currentIndex,
                                    false,
                                    &ok)
                                    .trimmed();

    if (!ok || teacherName.isEmpty())
    {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageLayout(
        QPageLayout(
            QPageSize(QPageSize::A4),
            QPageLayout::Portrait,
            QMarginsF(8, 8, 8, 8)));
    printer.setDocName("講師向け授業一覧");

    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle("講師向け授業一覧 - 印刷プレビュー");

    const QDate date = QDate::currentDate();
    connect(
        &preview,
        &QPrintPreviewDialog::paintRequested,
        this,
        [this, teacherName, date](QPrinter *previewPrinter)
        {
            renderTeacherDailyReportForPrint(previewPrinter, teacherName, date);
        });

    preview.exec();
}

void MainWindow::showSalaryStatementPrintPreview()
{
    updateCell();

    QStringList teacherNames;

    for (const TeacherData &teacher : teachers)
    {
        const QString name = teacher.name.trimmed();

        if (!name.isEmpty() && !teacherNames.contains(name))
        {
            teacherNames.append(name);
        }
    }

    if (teacherNames.isEmpty())
    {
        QMessageBox::information(this, "給与明細書", "講師が登録されていません。");
        return;
    }

    const QString currentTeacher = ui->teacherComboBox->currentText().trimmed();
    int currentIndex = qMax(0, teacherNames.indexOf(currentTeacher));

    bool ok = false;
    const QString teacherName = QInputDialog::getItem(
                                    this,
                                    "給与明細書",
                                    "講師を選択してください",
                                    teacherNames,
                                    currentIndex,
                                    false,
                                    &ok)
                                    .trimmed();

    if (!ok || teacherName.isEmpty())
    {
        return;
    }

    const QString defaultMonth =
        QDate::currentDate().toString("yyyy-MM");
    const QString monthText = QInputDialog::getText(
                                  this,
                                  "給与明細書",
                                  "対象月を入力してください（例: 2026-02）",
                                  QLineEdit::Normal,
                                  defaultMonth,
                                  &ok)
                                  .trimmed();

    if (!ok || monthText.isEmpty())
    {
        return;
    }

    const QDate month =
        QDate::fromString(monthText + "-01", "yyyy-MM-dd");

    if (!month.isValid())
    {
        QMessageBox::warning(this, "給与明細書", "対象月の形式が正しくありません。");
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setPageLayout(
        QPageLayout(
            QPageSize(QPageSize::A4),
            QPageLayout::Portrait,
            QMarginsF(8, 8, 8, 8)));
    printer.setDocName("給与支払明細書");

    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle("給与支払明細書 - 印刷プレビュー");

    connect(
        &preview,
        &QPrintPreviewDialog::paintRequested,
        this,
        [this, teacherName, month](QPrinter *previewPrinter)
        {
            renderSalaryStatementForPrint(previewPrinter, teacherName, month);
        });

    preview.exec();
}

void MainWindow::showGuidanceReportPrintPreview()
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setPageLayout(
        QPageLayout(
            QPageSize(QPageSize::A4),
            QPageLayout::Portrait,
            QMarginsF(8, 8, 8, 8)));
    printer.setDocName("指導報告書");

    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle("指導報告書 - 印刷プレビュー");

    connect(
        &preview,
        &QPrintPreviewDialog::paintRequested,
        this,
        &MainWindow::renderGuidanceReportFormatForPrint);

    preview.exec();
}

void MainWindow::copyStudentScheduleToClipboard()
{
    updateCell();

    QStringList gradeNames;

    for (const GradeStudents &gradeStudents : allStudents)
    {
        if (!gradeStudents.students.isEmpty())
        {
            gradeNames.append(gradeStudents.Grade);
        }
    }

    if (gradeNames.isEmpty())
    {
        QMessageBox::information(this, "生徒予定表", "生徒が登録されていません。");
        return;
    }

    bool ok = false;
    const QString grade = QInputDialog::getItem(
                              this,
                              "生徒予定表",
                              "学年を選択してください",
                              gradeNames,
                              0,
                              false,
                              &ok)
                              .trimmed();

    if (!ok || grade.isEmpty())
    {
        return;
    }

    QStringList studentNames;

    for (const GradeStudents &gradeStudents : allStudents)
    {
        if (gradeStudents.Grade != grade)
        {
            continue;
        }

        for (const StudentData &student : gradeStudents.students)
        {
            const QString name = student.Name.trimmed();

            if (!name.isEmpty())
            {
                studentNames.append(name);
            }
        }

        break;
    }

    if (studentNames.isEmpty())
    {
        QMessageBox::information(this, "生徒予定表", "選択した学年に生徒が登録されていません。");
        return;
    }

    const QString studentName = QInputDialog::getItem(
                                    this,
                                    "生徒予定表",
                                    "生徒を選択してください",
                                    studentNames,
                                    0,
                                    false,
                                    &ok)
                                    .trimmed();

    if (!ok || studentName.isEmpty())
    {
        return;
    }

    const QString text = studentScheduleText(grade, studentName);

    if (text.isEmpty())
    {
        QMessageBox::information(this, "生徒予定表", "予定が見つかりませんでした。");
        return;
    }

    QApplication::clipboard()->setText(text);
    statusBar()->showMessage("生徒予定表をクリップボードにコピーしました", 2000);
}

void MainWindow::renderScheduleForPrint(QPrinter *printer)
{
    QPainter painter(printer);

    if (!painter.isActive())
    {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, false);

    const int totalTeacherColumns = totalScheduleTeacherColumns();
    if (totalTeacherColumns == 0 || tableRowCount() == 0)
    {
        return;
    }

    const QRectF area = schedulePrintContentRect(printer);

    QFont timeFont = painter.font();
    timeFont.setPointSize(9);
    QFontMetrics timeMetrics(timeFont);

    int longestTimeWidth = 0;

    for (const QString &period : periods)
    {
        longestTimeWidth = qMax(
            longestTimeWidth,
            timeMetrics.horizontalAdvance(period));
    }

    const qreal timeColumnWidth = longestTimeWidth + 100;
    const qreal dayHeaderHeight = 100;
    const qreal teacherHeaderHeight = 100;
    const qreal headerHeight = dayHeaderHeight + teacherHeaderHeight;
    const qreal teacherColumnWidth =
        (area.width() - timeColumnWidth) / totalTeacherColumns;
    const qreal studentRowHeight =
        (area.height() - headerHeight) / tableRowCount();

    if (teacherColumnWidth <= 0 || studentRowHeight <= 0)
    {
        return;
    }

    drawSchedulePrintHeader(
        &painter,
        area,
        timeColumnWidth,
        teacherColumnWidth,
        dayHeaderHeight,
        teacherHeaderHeight);

    drawSchedulePrintBody(
        &painter,
        area,
        timeColumnWidth,
        teacherColumnWidth,
        headerHeight,
        studentRowHeight);
}

bool MainWindow::findStudentData(
    const QString &grade,
    const QString &studentName,
    StudentData *student) const
{
    for (const GradeStudents &gradeStudents : allStudents)
    {
        if (gradeStudents.Grade != grade)
        {
            continue;
        }

        for (const StudentData &candidate : gradeStudents.students)
        {
            if (candidate.Name == studentName)
            {
                if (student != nullptr)
                {
                    *student = candidate;
                }

                return true;
            }
        }
    }

    return false;
}

bool MainWindow::findNextLessonForStudent(
    const LessonRecord &baseLesson,
    LessonRecord *nextLesson) const
{
    const QDate startMonday = mondayOf(baseLesson.date);

    for (int weekOffset = 0; weekOffset < 8; ++weekOffset)
    {
        const QDate targetMonday = startMonday.addDays(weekOffset * 7);
        QVector<QVector<TeacherColumn>> loadedSchedule;
        QDate fileMonday;

        if (targetMonday == scheduleMonday)
        {
            loadedSchedule = schedule;
            fileMonday = scheduleMonday;
        }
        else if (!loadScheduleDataFromFile(targetMonday, &fileMonday, &loadedSchedule))
        {
            continue;
        }

        QVector<LessonRecord> entries =
            scheduleEntriesFor(fileMonday, loadedSchedule);
        std::sort(entries.begin(), entries.end(), lessonRecordLess);

        for (const LessonRecord &entry : entries)
        {
            if (entry.studentGrade != baseLesson.studentGrade ||
                entry.studentName != baseLesson.studentName)
            {
                continue;
            }

            const bool laterDate = entry.date > baseLesson.date;
            const bool laterPeriod =
                entry.date == baseLesson.date &&
                entry.periodIndex > baseLesson.periodIndex;

            if (!laterDate && !laterPeriod)
            {
                continue;
            }

            if (nextLesson != nullptr)
            {
                *nextLesson = entry;
            }

            return true;
        }
    }

    return false;
}

QString MainWindow::studentScheduleText(
    const QString &grade,
    const QString &studentName) const
{
    QVector<LessonRecord> entries = scheduleEntries();
    std::sort(entries.begin(), entries.end(), lessonRecordLess);

    QStringList lines;

    for (const LessonRecord &entry : entries)
    {
        if (entry.studentGrade != grade || entry.studentName != studentName)
        {
            continue;
        }

        lines << QString("%1 %2 %3 %4")
                     .arg(entry.date.toString("MM/dd"))
                     .arg(dayNameText(entry.day))
                     .arg(entry.period)
                     .arg(entry.subject.trimmed());
    }

    return lines.join('\n');
}

void MainWindow::renderTeacherDailyReportForPrint(
    QPrinter *printer,
    const QString &teacherName,
    const QDate &date)
{
    QPainter painter(printer);

    if (!painter.isActive())
    {
        return;
    }

    const QRectF pageRect =
        printer->pageLayout().paintRectPixels(printer->resolution());
    const qreal scale = qMax(1.0, printer->resolution() / 96.0);
    const QRectF area = pageRect.adjusted(60 * scale, 60 * scale, -60 * scale, -60 * scale);

    QVector<QVector<TeacherColumn>> loadedSchedule;
    QDate loadedMonday;
    const QDate targetMonday = mondayOf(date);

    if (targetMonday == scheduleMonday)
    {
        loadedSchedule = schedule;
        loadedMonday = scheduleMonday;
    }
    else
    {
        loadScheduleDataFromFile(targetMonday, &loadedMonday, &loadedSchedule);
    }

    QVector<LessonRecord> entries =
        scheduleEntriesFor(loadedMonday, loadedSchedule);
    QVector<LessonRecord> todayEntries;

    for (const LessonRecord &entry : entries)
    {
        if (entry.date == date && entry.teacherName == teacherName)
        {
            todayEntries.append(entry);
        }
    }

    std::sort(todayEntries.begin(), todayEntries.end(), lessonRecordLess);

    auto drawPageHeader = [&]()
    {
        drawSchedulePrintText(
            &painter,
            QRectF(area.left(), area.top(), area.width(), 36 * scale),
            QString("講師向け授業一覧　%1　%2")
                .arg(date.toString("yyyy/MM/dd"))
                .arg(teacherName),
            Qt::AlignLeft | Qt::AlignVCenter,
            true);
    };

    drawPageHeader();

    qreal y = area.top() + 52 * scale;
    const qreal cardHeight = 150 * scale;
    const qreal gap = 18 * scale;

    if (todayEntries.isEmpty())
    {
        drawSchedulePrintText(
            &painter,
            QRectF(area.left(), y, area.width(), 80 * scale),
            "今日の授業は見つかりませんでした。",
            Qt::AlignCenter);
        return;
    }

    for (const LessonRecord &entry : todayEntries)
    {
        if (y + cardHeight > area.bottom())
        {
            printer->newPage();
            y = area.top() + 52 * scale;
            drawPageHeader();
        }

        const QRectF card(area.left(), y, area.width(), cardHeight);
        painter.setPen(QPen(Qt::black, 2));
        painter.drawRect(card);

        StudentData student;
        findStudentData(entry.studentGrade, entry.studentName, &student);

        LessonRecord nextLesson;
        const bool hasNext = findNextLessonForStudent(entry, &nextLesson);
        const QString nextLessonText =
            hasNext
                ? QString("%1 %2 %3 %4")
                      .arg(nextLesson.date.toString("MM/dd"))
                      .arg(dayNameText(nextLesson.day))
                      .arg(nextLesson.period)
                      .arg(nextLesson.subject)
                : QString("未定");

        const QRectF left = card.adjusted(14 * scale, 10 * scale, -card.width() * 0.48, -10 * scale);
        const QRectF right = card.adjusted(card.width() * 0.54, 10 * scale, -14 * scale, -10 * scale);

        drawSchedulePrintText(
            &painter,
            QRectF(left.left(), left.top(), left.width(), 26 * scale),
            QString("%1　%2 %3")
                .arg(entry.period)
                .arg(entry.studentGrade)
                .arg(entry.studentName),
            Qt::AlignLeft | Qt::AlignVCenter,
            true);

        drawSchedulePrintText(
            &painter,
            QRectF(left.left(), left.top() + 34 * scale, left.width(), 26 * scale),
            QString("科目：%1").arg(entry.subject),
            Qt::AlignLeft | Qt::AlignVCenter);
        drawSchedulePrintText(
            &painter,
            QRectF(left.left(), left.top() + 64 * scale, left.width(), 26 * scale),
            QString("学校：%1").arg(student.school),
            Qt::AlignLeft | Qt::AlignVCenter);
        drawSchedulePrintText(
            &painter,
            QRectF(left.left(), left.top() + 94 * scale, left.width(), 34 * scale),
            QString("次回：%1").arg(nextLessonText),
            Qt::AlignLeft | Qt::AlignVCenter);

        drawSchedulePrintText(
            &painter,
            QRectF(right.left(), right.top(), right.width(), 62 * scale),
            QString("授業メモ：%1").arg(entry.memo),
            Qt::AlignLeft | Qt::AlignTop);
        drawSchedulePrintText(
            &painter,
            QRectF(right.left(), right.top() + 70 * scale, right.width(), 62 * scale),
            QString("生徒メモ：%1").arg(student.memo),
            Qt::AlignLeft | Qt::AlignTop);

        y += cardHeight + gap;
    }
}

void MainWindow::renderSalaryStatementForPrint(
    QPrinter *printer,
    const QString &teacherName,
    const QDate &month)
{
    QPainter painter(printer);

    if (!painter.isActive())
    {
        return;
    }

    const QRectF pageRect =
        printer->pageLayout().paintRectPixels(printer->resolution());
    const qreal scale = qMax(1.0, printer->resolution() / 96.0);
    const QRectF area = pageRect.adjusted(
        50 * scale,
        45 * scale,
        -50 * scale,
        -45 * scale);
    const QDate monthStart(month.year(), month.month(), 1);
    const QDate monthEnd(month.year(), month.month(), month.daysInMonth());

    QMap<QDate, SalaryDaySummary> summaries;
    QDate day = monthStart;

    while (day <= monthEnd)
    {
        summaries.insert(day, SalaryDaySummary());
        day = day.addDays(1);
    }

    QDate weekMonday = mondayOf(monthStart);

    while (weekMonday <= monthEnd)
    {
        QVector<QVector<TeacherColumn>> loadedSchedule;
        QDate loadedMonday;

        if (weekMonday == scheduleMonday)
        {
            loadedSchedule = schedule;
            loadedMonday = scheduleMonday;
        }
        else
        {
            loadScheduleDataFromFile(weekMonday, &loadedMonday, &loadedSchedule);
        }

        QMap<QString, QVector<LessonRecord>> lessonSlots;

        for (const LessonRecord &entry : scheduleEntriesFor(loadedMonday, loadedSchedule))
        {
            if (entry.teacherName != teacherName ||
                entry.date < monthStart ||
                entry.date > monthEnd)
            {
                continue;
            }

            const QString key = QString("%1|%2|%3")
                                    .arg(entry.date.toString("yyyyMMdd"))
                                    .arg(entry.teacherIndex)
                                    .arg(entry.periodIndex);
            lessonSlots[key].append(entry);
        }

        for (const QVector<LessonRecord> &slotEntries : lessonSlots)
        {
            if (slotEntries.isEmpty())
            {
                continue;
            }

            SalaryDaySummary &summary = summaries[slotEntries.first().date];

            if (slotEntries.size() >= 2)
            {
                ++summary.oneOnTwoCount;
            }
            else
            {
                ++summary.oneOnOneCount;
            }

            for (const LessonRecord &entry : slotEntries)
            {
                if (entry.studentGrade.contains("高"))
                {
                    ++summary.highSchoolAllowanceCount;
                }
            }
        }

        weekMonday = weekMonday.addDays(7);
    }

    int grandTotal = 0;

    for (QMap<QDate, SalaryDaySummary>::iterator it = summaries.begin();
         it != summaries.end();
         ++it)
    {
        SalaryDaySummary &summary = it.value();

        if (summary.oneOnTwoCount > 0 ||
            summary.oneOnOneCount > 0 ||
            summary.highSchoolAllowanceCount > 0)
        {
            summary.businessPay = salaryBusinessPay;
            summary.transportPay = salaryTransportPay;
        }

        grandTotal += summary.total(
            salaryOneOnTwoRate,
            salaryOneOnOneRate,
            salaryHighSchoolAllowance);
    }

    auto drawBoxText = [&painter, this](
                           const QRectF &rect,
                           const QString &text,
                           int alignment,
                           bool bold = false)
    {
        painter.drawRect(rect);
        drawSchedulePrintText(
            &painter,
            rect.adjusted(3, 1, -3, -1),
            text,
            alignment,
            bold);
    };

    drawSchedulePrintText(
        &painter,
        QRectF(area.left(), area.top(), area.width() * 0.35, 42 * scale),
        QString("%1年 %2月分").arg(month.year()).arg(month.month()),
        Qt::AlignCenter,
        true);
    drawSchedulePrintText(
        &painter,
        QRectF(area.left() + area.width() * 0.35, area.top(), area.width() * 0.65, 42 * scale),
        "給与支払明細書",
        Qt::AlignCenter,
        true);

    const qreal infoTop = area.top() + 44 * scale;
    drawSchedulePrintText(
        &painter,
        QRectF(area.left(), infoTop, area.width() * 0.5, 24 * scale),
        QString("給与計算期間　%1　～　%2")
            .arg(monthStart.toString("yyyy年M月d日"))
            .arg(monthEnd.toString("yyyy年M月d日")),
        Qt::AlignLeft | Qt::AlignVCenter);
    drawSchedulePrintText(
        &painter,
        QRectF(area.left(), infoTop + 26 * scale, area.width() * 0.5, 24 * scale),
        "片倉町駅前教室",
        Qt::AlignLeft | Qt::AlignVCenter);
    drawSchedulePrintText(
        &painter,
        QRectF(area.left() + area.width() * 0.55, infoTop + 26 * scale, area.width() * 0.45, 24 * scale),
        QString("講師　%1").arg(teacherName),
        Qt::AlignLeft | Qt::AlignVCenter);

    const qreal tableTop = area.top() + 100 * scale;
    const qreal rowHeight = 20 * scale;
    const QVector<qreal> widths = {
        82 * scale,
        54 * scale,
        58 * scale,
        54 * scale,
        58 * scale,
        74 * scale,
        74 * scale,
        74 * scale,
        96 * scale};
    QStringList headers = {
        "勤務日",
        "1:2授業",
        "コマ給",
        "1:1授業",
        "コマ給",
        "高校生手当",
        "業務給",
        "交通費",
        "小計"};

    qreal x = area.left();

    for (int i = 0; i < headers.size(); ++i)
    {
        drawBoxText(
            QRectF(x, tableTop, widths[i], rowHeight),
            headers[i],
            Qt::AlignCenter,
            true);
        x += widths[i];
    }

    qreal y = tableTop + rowHeight;

    for (int dayIndex = 1; dayIndex <= month.daysInMonth(); ++dayIndex)
    {
        const QDate currentDay(month.year(), month.month(), dayIndex);
        const SalaryDaySummary summary = summaries.value(currentDay);
        const int subtotal = summary.total(
            salaryOneOnTwoRate,
            salaryOneOnOneRate,
            salaryHighSchoolAllowance);
        QStringList row = {
            currentDay.toString("M月d日"),
            summary.oneOnTwoCount > 0 ? QString::number(summary.oneOnTwoCount) : QString(),
            summary.oneOnTwoCount > 0 ? QString::number(salaryOneOnTwoRate) : QString(),
            summary.oneOnOneCount > 0 ? QString::number(summary.oneOnOneCount) : QString(),
            summary.oneOnOneCount > 0 ? QString::number(salaryOneOnOneRate) : QString(),
            summary.highSchoolAllowanceCount > 0
                ? QString::number(summary.highSchoolAllowanceCount * salaryHighSchoolAllowance)
                : QString(),
            summary.businessPay > 0 ? QString::number(summary.businessPay) : QString(),
            summary.transportPay > 0 ? QString::number(summary.transportPay) : QString(),
            subtotal > 0 ? moneyText(subtotal) : QString("-")};

        x = area.left();

        for (int i = 0; i < row.size(); ++i)
        {
            drawBoxText(
                QRectF(x, y, widths[i], rowHeight),
                row[i],
                i == 0 ? Qt::AlignCenter : Qt::AlignRight | Qt::AlignVCenter);
            x += widths[i];
        }

        y += rowHeight;
    }

    const QRectF totalLabel(area.left() + 420 * scale, y + 8 * scale, 100 * scale, 24 * scale);
    const QRectF totalBox(area.left() + 520 * scale, y + 8 * scale, 128 * scale, 24 * scale);
    drawSchedulePrintText(
        &painter,
        totalLabel,
        "支給額合計",
        Qt::AlignRight | Qt::AlignVCenter);
    drawBoxText(
        totalBox,
        QString("￥ %1").arg(moneyText(grandTotal)),
        Qt::AlignRight | Qt::AlignVCenter,
        true);

    y += 56 * scale;
    drawSchedulePrintText(
        &painter,
        QRectF(area.left(), y, area.width(), 24 * scale),
        "控除",
        Qt::AlignLeft | Qt::AlignVCenter);

    QStringList deductionHeaders = {
        "住民税", "所得税", "厚生年金", "健康保険", "雇用保険"};
    x = area.left();

    for (const QString &header : deductionHeaders)
    {
        drawBoxText(QRectF(x, y + 26 * scale, 96 * scale, rowHeight), header, Qt::AlignCenter);
        drawBoxText(QRectF(x, y + 26 * scale + rowHeight, 96 * scale, rowHeight), "0", Qt::AlignCenter);
        x += 96 * scale;
    }

    const QRectF deductionTotalLabel(area.left() + 420 * scale, y + 58 * scale, 100 * scale, 24 * scale);
    const QRectF deductionTotalBox(area.left() + 520 * scale, y + 58 * scale, 128 * scale, 24 * scale);
    drawSchedulePrintText(
        &painter,
        deductionTotalLabel,
        "控除額合計",
        Qt::AlignRight | Qt::AlignVCenter);
    drawBoxText(
        deductionTotalBox,
        "￥ -",
        Qt::AlignRight | Qt::AlignVCenter,
        true);

    const QRectF payLabel(area.left() + 360 * scale, area.bottom() - 50 * scale, 120 * scale, 36 * scale);
    const QRectF payBox(area.left() + 490 * scale, area.bottom() - 58 * scale, 160 * scale, 46 * scale);
    drawSchedulePrintText(
        &painter,
        payLabel,
        "差引支給額",
        Qt::AlignRight | Qt::AlignVCenter);
    drawBoxText(
        payBox,
        QString("￥ %1").arg(moneyText(grandTotal)),
        Qt::AlignRight | Qt::AlignVCenter,
        true);
}

void MainWindow::renderGuidanceReportFormatForPrint(QPrinter *printer)
{
    QPainter painter(printer);

    if (!painter.isActive())
    {
        return;
    }

    const QRectF pageRect =
        printer->pageLayout().paintRectPixels(printer->resolution());
    const qreal scale = qMax(1.0, printer->resolution() / 96.0);
    const QRectF area = pageRect.adjusted(
        55 * scale,
        55 * scale,
        -55 * scale,
        -55 * scale);
    const qreal gap = 28 * scale;
    const qreal reportHeight = (area.height() - gap) / 2.0;

    auto drawReport = [this, &painter, scale](const QRectF &rect)
    {
        const QPen borderPen(Qt::black, 2);
        const QPen normalPen(Qt::black, 1);
        const QPen gridPen(QColor(220, 220, 220), 1);
        const qreal left = rect.left() + 14 * scale;
        const qreal right = rect.right() - 14 * scale;
        const qreal width = right - left;
        const qreal labelHeight = 18 * scale;
        const qreal rowHeight = 26 * scale;

        auto drawGrid = [&](const QRectF &box)
        {
            painter.save();
            painter.setPen(gridPen);

            const qreal cell = 12 * scale;

            for (qreal x = box.left() + cell; x < box.right(); x += cell)
            {
                painter.drawLine(QPointF(x, box.top()), QPointF(x, box.bottom()));
            }

            for (qreal y = box.top() + cell; y < box.bottom(); y += cell)
            {
                painter.drawLine(QPointF(box.left(), y), QPointF(box.right(), y));
            }

            painter.drawLine(
                QPointF(box.center().x(), box.top()),
                QPointF(box.center().x(), box.bottom()));
            painter.drawLine(
                QPointF(box.left(), box.center().y()),
                QPointF(box.right(), box.center().y()));
            painter.restore();
        };

        auto drawBox = [&](const QRectF &box, bool grid = true)
        {
            if (grid)
            {
                drawGrid(box);
            }

            painter.setPen(normalPen);
            painter.drawRect(box);
        };

        auto drawLabel = [&](const QRectF &box, const QString &text)
        {
            drawSchedulePrintText(
                &painter,
                box,
                text,
                Qt::AlignLeft | Qt::AlignVCenter,
                true);
        };

        painter.setPen(borderPen);
        painter.drawRect(rect);

        drawSchedulePrintText(
            &painter,
            QRectF(rect.left(), rect.top() + 5 * scale, rect.width(), 24 * scale),
            "指導報告書",
            Qt::AlignCenter,
            true);

        qreal y = rect.top() + 32 * scale;

        const qreal subjectWidth = width * 0.34;
        const qreal gradeWidth = width * 0.18;
        const qreal nameWidth = width - subjectWidth - gradeWidth - 16 * scale;
        const QRectF subjectLabel(left, y, 40 * scale, rowHeight);
        const QRectF subjectBox(subjectLabel.right(), y, subjectWidth - 40 * scale, rowHeight);
        const QRectF gradeLabel(subjectBox.right() + 8 * scale, y, 40 * scale, rowHeight);
        const QRectF gradeBox(gradeLabel.right(), y, gradeWidth - 40 * scale, rowHeight);
        const QRectF nameLabel(gradeBox.right() + 8 * scale, y, 48 * scale, rowHeight);
        const QRectF nameBox(nameLabel.right(), y, nameWidth - 48 * scale, rowHeight);

        drawLabel(subjectLabel, "教科");
        drawBox(subjectBox);
        drawLabel(gradeLabel, "学年");
        drawBox(gradeBox);
        drawLabel(nameLabel, "氏名");
        drawBox(nameBox);
        y += rowHeight + 5 * scale;

        const QRectF dateLabel(left, y, 40 * scale, rowHeight);
        const QRectF dateBox(dateLabel.right(), y, 118 * scale, rowHeight);
        const QRectF timeLabel(dateBox.right() + 8 * scale, y, 40 * scale, rowHeight);
        const QRectF timeBox(timeLabel.right(), y, 150 * scale, rowHeight);
        const QRectF attendanceBox(timeBox.right() + 8 * scale, y, right - timeBox.right() - 8 * scale, rowHeight);

        drawLabel(dateLabel, "日付");
        drawBox(dateBox, false);
        painter.drawLine(dateBox.topLeft(), dateBox.bottomRight());
        drawLabel(timeLabel, "時刻");
        drawBox(timeBox, false);
        drawSchedulePrintText(
            &painter,
            timeBox.adjusted(8 * scale, 0, -8 * scale, 0),
            "   ：      ～      ：   ",
            Qt::AlignCenter);
        drawBox(attendanceBox, false);
        drawSchedulePrintText(
            &painter,
            attendanceBox.adjusted(8 * scale, 0, -8 * scale, 0),
            "出席・欠席        分",
            Qt::AlignLeft | Qt::AlignVCenter);
        y += rowHeight + 8 * scale;

        drawLabel(QRectF(left, y, width, labelHeight), "指導内容");
        y += labelHeight;

        const QRectF unitLabel(left, y, 58 * scale, rowHeight);
        const QRectF unitBox(unitLabel.right(), y, right - unitLabel.right(), rowHeight);
        drawSchedulePrintText(
            &painter,
            unitLabel,
            "単元名",
            Qt::AlignLeft | Qt::AlignVCenter);
        drawBox(unitBox);
        y += rowHeight;

        const qreal materialLabelWidth = 72 * scale;
        const qreal pageWidth = 92 * scale;
        const qreal problemWidth = 116 * scale;
        const qreal materialWidth = width - materialLabelWidth - pageWidth - problemWidth;

        for (int i = 0; i < 3; ++i)
        {
            const QRectF materialBox(left + materialLabelWidth, y, materialWidth, rowHeight);
            const QRectF pageBox(materialBox.right(), y, pageWidth, rowHeight);
            const QRectF problemBox(pageBox.right(), y, problemWidth, rowHeight);

            if (i == 0)
            {
                drawSchedulePrintText(
                    &painter,
                    QRectF(left, y, materialLabelWidth, rowHeight),
                    "教材名",
                    Qt::AlignLeft | Qt::AlignVCenter);
                drawSchedulePrintText(
                    &painter,
                    pageBox.adjusted(3 * scale, 0, -3 * scale, 0),
                    "ページ",
                    Qt::AlignCenter,
                    true);
                drawSchedulePrintText(
                    &painter,
                    problemBox.adjusted(3 * scale, 0, -3 * scale, 0),
                    "問題番号",
                    Qt::AlignCenter,
                    true);
            }

            drawBox(materialBox);
            drawBox(pageBox);
            drawBox(problemBox);
            y += rowHeight;
        }

        y += 6 * scale;
        drawLabel(QRectF(left, y, width, labelHeight), "宿題実施状況");
        y += labelHeight;

        const qreal metricSlot = width / 4.0;
        const qreal smallLabel = 56 * scale;
        const qreal smallBox = 38 * scale;
        const QStringList homeworkLabels = {
            "宿題実施", "正答率", "小テスト", "理解度"};
        const QStringList homeworkSuffixes = {
            " / 10", " %", " / 10", " / 5"};
        qreal x = left;

        for (int i = 0; i < homeworkLabels.size(); ++i)
        {
            drawSchedulePrintText(
                &painter,
                QRectF(x, y, smallLabel, rowHeight),
                homeworkLabels[i],
                Qt::AlignLeft | Qt::AlignVCenter);
            const QRectF valueBox(x + smallLabel, y, smallBox, rowHeight);
            drawBox(valueBox, false);
            drawSchedulePrintText(
                &painter,
                QRectF(valueBox.right() + 2 * scale, y, 42 * scale, rowHeight),
                homeworkSuffixes[i],
                Qt::AlignLeft | Qt::AlignVCenter);
            x += metricSlot;
        }
        y += rowHeight + 6 * scale;

        const qreal memoBottom = rect.bottom() - 40 * scale;
        const QRectF memoLabel(left, y, 76 * scale, memoBottom - y);
        const QRectF memoBox(memoLabel.right(), y, right - memoLabel.right(), memoBottom - y);
        drawSchedulePrintText(
            &painter,
            memoLabel,
            "備考",
            Qt::AlignLeft | Qt::AlignTop);
        drawBox(memoBox);

        const QRectF nextLabel(left, rect.bottom() - 32 * scale, 76 * scale, 24 * scale);
        const QRectF nextBox(nextLabel.right(), nextLabel.top(), right - nextLabel.right(), 24 * scale);
        drawSchedulePrintText(
            &painter,
            nextLabel,
            "次回予定",
            Qt::AlignLeft | Qt::AlignVCenter,
            true);
        drawBox(nextBox);
    };

    drawReport(QRectF(area.left(), area.top(), area.width(), reportHeight));
    drawReport(QRectF(area.left(), area.top() + reportHeight + gap, area.width(), reportHeight));
}

int MainWindow::totalScheduleTeacherColumns() const
{
    int totalTeacherColumns = 0;

    for (const QVector<TeacherColumn> &daySchedule : schedule)
    {
        totalTeacherColumns += daySchedule.size();
    }

    return totalTeacherColumns;
}

QRectF MainWindow::schedulePrintContentRect(QPrinter *printer) const
{
    const QRect pageRect =
        printer->pageLayout().paintRectPixels(printer->resolution());
    const qreal dpiScale = qMax(1.0, printer->resolution() / 96.0);
    const int maxSourceWidth = qMax(
        qMax(scheduleVerticalLineWidth, scheduleHorizontalLineWidth),
        qMax(scheduleVerticalSectionLineWidth, scheduleHorizontalSectionLineWidth));
    const qreal lineInset =
        maxSourceWidth * dpiScale * schedulePrintLineWidthPercent / 100.0;
    const qreal margin = 40.0 + lineInset;
    QRectF area = QRectF(pageRect).adjusted(margin, margin, -margin, -margin);
    const qreal sizeScale =
        qBound(50, schedulePrintSizePercent, 100) / 100.0;
    const qreal horizontalPadding =
        area.width() * (1.0 - sizeScale) / 2.0;
    const qreal verticalPadding =
        area.height() * (1.0 - sizeScale) / 2.0;

    return area.adjusted(
        horizontalPadding,
        verticalPadding,
        -horizontalPadding,
        -verticalPadding);
}

qreal MainWindow::schedulePrintLineWidth(QPainter *painter, int width) const
{
    if (width <= 0)
    {
        return 0.0;
    }

    const QPaintDevice *device = painter->device();
    const qreal dpiScale =
        device == nullptr
            ? 1.0
            : qMax(1.0, device->logicalDpiX() / 96.0);

    return width * dpiScale * schedulePrintLineWidthPercent / 100.0;
}

QColor MainWindow::schedulePrintColor(const QString &colorName) const
{
    QColor color(colorName);

    if (!color.isValid())
    {
        color = Qt::black;
    }

    return color.darker(qMax(1, schedulePrintDarknessPercent));
}

void MainWindow::drawSchedulePrintText(
    QPainter *painter,
    const QRectF &rect,
    const QString &text,
    int alignment,
    bool bold) const
{
    const QFont previousFont = painter->font();
    QFont currentFont = previousFont;
    currentFont.setBold(bold);

    int pointSize = bold ? 11 : 9;

    while (pointSize >= 5)
    {
        currentFont.setPointSize(pointSize);

        QFontMetrics metrics(currentFont);
        const QRect textRect = metrics.boundingRect(
            rect.toRect(),
            alignment | Qt::TextWordWrap,
            text);

        if (textRect.width() <= rect.width() &&
            textRect.height() <= rect.height())
        {
            break;
        }

        --pointSize;
    }

    painter->setFont(currentFont);
    const QPen previousPen = painter->pen();
    painter->setPen(QPen(schedulePrintColor(scheduleTextColor)));
    painter->drawText(
        rect,
        alignment | Qt::TextWordWrap,
        text);
    painter->setPen(previousPen);
    painter->setFont(previousFont);
}

void MainWindow::fillSchedulePrintRowBackground(
    QPainter *painter,
    const QRectF &rect,
    int tableRow) const
{
    if ((tableRow + 1) % 2 != 1)
    {
        return;
    }

    painter->fillRect(rect, schedulePrintColor(scheduleOddRowColor));
}

void MainWindow::drawSchedulePrintLines(
    QPainter *painter,
    const QRectF &rect,
    bool drawRightSection,
    bool drawBottomSection) const
{
    const QColor verticalLineColor =
        schedulePrintColor(scheduleVerticalLineColor);
    const QColor horizontalLineColor =
        schedulePrintColor(scheduleHorizontalLineColor);
    const qreal verticalLineWidth =
        schedulePrintLineWidth(painter, scheduleVerticalLineWidth);
    const qreal horizontalLineWidth =
        schedulePrintLineWidth(painter, scheduleHorizontalLineWidth);

    if (verticalLineWidth > 0)
    {
        const qreal rightX = rect.right() - verticalLineWidth / 2.0;

        if (!drawRightSection)
        {
            painter->setPen(QPen(verticalLineColor, verticalLineWidth));
            painter->drawLine(
                QPointF(rightX, rect.top()),
                QPointF(rightX, rect.bottom()));
        }
    }

    if (horizontalLineWidth > 0)
    {
        const qreal bottomY = rect.bottom() - horizontalLineWidth / 2.0;

        if (!drawBottomSection)
        {
            painter->setPen(QPen(horizontalLineColor, horizontalLineWidth));
            painter->drawLine(
                QPointF(rect.left(), bottomY),
                QPointF(rect.right(), bottomY));
        }
    }

    // 太字化処理、やめたかったら以下ブロックをコメントアウトする。
    
    const QColor verticalSectionLineColor =
        schedulePrintColor(scheduleVerticalSectionLineColor);
    const QColor horizontalSectionLineColor =
        schedulePrintColor(scheduleHorizontalSectionLineColor);
    const qreal verticalSectionLineWidth =
        schedulePrintLineWidth(painter, scheduleVerticalSectionLineWidth);
    const qreal horizontalSectionLineWidth =
        schedulePrintLineWidth(painter, scheduleHorizontalSectionLineWidth);

    if (drawRightSection && verticalSectionLineWidth > 0)
    {
        const qreal rightX = rect.right() - verticalSectionLineWidth / 2.0;

        painter->setPen(QPen(verticalSectionLineColor, verticalSectionLineWidth));
        painter->drawLine(
            QPointF(rightX, rect.top()),
            QPointF(rightX, rect.bottom()));
    }

    if (drawBottomSection && horizontalSectionLineWidth > 0)
    {
        const qreal bottomY = rect.bottom() - horizontalSectionLineWidth / 2.0;

        painter->setPen(QPen(horizontalSectionLineColor, horizontalSectionLineWidth));
        painter->drawLine(
            QPointF(rect.left(), bottomY),
            QPointF(rect.right(), bottomY));
    }

}

void MainWindow::drawSchedulePrintHeader(
    QPainter *painter,
    const QRectF &area,
    qreal timeColumnWidth,
    qreal teacherColumnWidth,
    qreal dayHeaderHeight,
    qreal teacherHeaderHeight) const
{
    const qreal headerHeight = dayHeaderHeight + teacherHeaderHeight;
    const QRectF timeHeaderRect(
        area.left(),
        area.top(),
        timeColumnWidth,
        headerHeight);

    drawSchedulePrintLines(painter, timeHeaderRect, true, true);
    drawSchedulePrintText(
        painter,
        timeHeaderRect.adjusted(3, 2, -3, -2),
        "時間",
        Qt::AlignCenter,
        true);

    qreal x = area.left() + timeColumnWidth;

    for (int dayIndex = 0; dayIndex < schedule.size(); ++dayIndex)
    {
        const QVector<TeacherColumn> &daySchedule = schedule[dayIndex];

        if (daySchedule.isEmpty())
        {
            continue;
        }

        const qreal dayWidth =
            teacherColumnWidth * daySchedule.size();
        const QRectF dayRect(
            x,
            area.top(),
            dayWidth,
            dayHeaderHeight);
        const QDate date = scheduleMonday.addDays(dayIndex);
        const QString dayText = QString("%1\t%2")
                                    .arg(date.toString("M/d"))
                                    .arg(days.value(dayIndex));

        drawSchedulePrintLines(painter, dayRect, true, true);
        drawSchedulePrintText(
            painter,
            dayRect.adjusted(3, 2, -3, -2),
            dayText,
            Qt::AlignCenter,
            true);

        for (int teacherIndex = 0;
             teacherIndex < daySchedule.size();
             ++teacherIndex)
        {
            const TeacherColumn &teacher = daySchedule[teacherIndex];
            const bool dayEndColumn =
                teacherIndex == daySchedule.size() - 1;
            const QRectF teacherRect(
                x + teacherColumnWidth * teacherIndex,
                area.top() + dayHeaderHeight,
                teacherColumnWidth,
                teacherHeaderHeight);
            const QString teacherName =
                teacher.teacherName.trimmed().isEmpty()
                    ? "講師未設定"
                    : teacher.teacherName;

            drawSchedulePrintLines(painter, teacherRect, dayEndColumn, true);
            drawSchedulePrintText(
                painter,
                teacherRect.adjusted(3, 2, -3, -2),
                teacherName,
                Qt::AlignCenter,
                true);
        }

        x += dayWidth;
    }
}

void MainWindow::drawSchedulePrintBody(
    QPainter *painter,
    const QRectF &area,
    qreal timeColumnWidth,
    qreal teacherColumnWidth,
    qreal headerHeight,
    qreal studentRowHeight) const
{
    for (int periodIndex = 0; periodIndex < periods.size(); ++periodIndex)
    {
        const qreal periodTop =
            area.top() +
            headerHeight +
            studentRowHeight * MaxStudentPerTeacher * periodIndex;
        const QRectF timeRect(
            area.left(),
            periodTop,
            timeColumnWidth,
            studentRowHeight * MaxStudentPerTeacher);

        drawSchedulePrintLines(painter, timeRect, true, true);
        drawSchedulePrintText(
            painter,
            timeRect.adjusted(3, 2, -3, -2),
            periods[periodIndex],
            Qt::AlignCenter);

        for (int studentIndex = 0;
             studentIndex < MaxStudentPerTeacher;
             ++studentIndex)
        {
            const int tableRow =
                tableRowOf(periodIndex, studentIndex);
            const bool periodEndRow =
                studentIndex == MaxStudentPerTeacher - 1;
            const qreal y =
                periodTop + studentRowHeight * studentIndex;
            qreal x = area.left() + timeColumnWidth;

            for (const QVector<TeacherColumn> &daySchedule : schedule)
            {
                for (int teacherIndex = 0;
                     teacherIndex < daySchedule.size();
                     ++teacherIndex)
                {
                    const TeacherColumn &teacher = daySchedule[teacherIndex];
                    const bool dayEndColumn =
                        teacherIndex == daySchedule.size() - 1;
                    const QRectF cellRect(
                        x,
                        y,
                        teacherColumnWidth,
                        studentRowHeight);

                    fillSchedulePrintRowBackground(
                        painter,
                        cellRect,
                        tableRow);
                    drawSchedulePrintLines(
                        painter,
                        cellRect,
                        dayEndColumn,
                        periodEndRow);

                    QString text;

                    if (periodIndex < teacher.lessons.size() &&
                        studentIndex < teacher.lessons[periodIndex].size())
                    {
                        text = cellTextFromData(
                            teacher.lessons[periodIndex][studentIndex]);
                    }

                    drawSchedulePrintText(
                        painter,
                        cellRect.adjusted(5, 4, -5, -4),
                        text,
                        Qt::AlignLeft | Qt::AlignVCenter);

                    x += teacherColumnWidth;
                }
            }
        }
    }
}
