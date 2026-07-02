#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
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
#include <QSpinBox>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <numeric>

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

QVector<TeacherDailyPayData> MainWindow::salaryDailyPayDefaults(
    const QString &teacherName,
    const QDate &month) const
{
    TeacherData teacher;
    teacher.name = teacherName;
    teacher.oneOnTwoRate = defaultSalaryOneOnTwoRate;
    teacher.oneOnOneRate = defaultSalaryOneOnOneRate;
    teacher.transportPay = defaultSalaryTransportPay;
    teacher.highSchoolAllowance = defaultSalaryHighSchoolAllowance;
    findTeacherData(teacherName, &teacher);

    const QDate monthStart(month.year(), month.month(), 1);
    const QDate monthEnd(month.year(), month.month(), month.daysInMonth());
    QMap<QDate, SalaryDaySummary> summaries;

    for (QDate day = monthStart; day <= monthEnd; day = day.addDays(1))
    {
        summaries.insert(day, SalaryDaySummary());
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

    QMap<QDate, TeacherDailyPayData> dailyPayByDate;

    for (QMap<QDate, SalaryDaySummary>::const_iterator it = summaries.cbegin();
         it != summaries.cend();
         ++it)
    {
        TeacherDailyPayData dailyPay;
        dailyPay.teacherName = teacherName;
        dailyPay.date = it.key();
        dailyPay.lessonCount = it.value().oneOnTwoCount + it.value().oneOnOneCount;
        dailyPay.highSchoolStudentCount = it.value().highSchoolAllowanceCount;
        dailyPay.businessPay = 0;
        dailyPay.transportPay = dailyPay.lessonCount > 0 ? teacher.transportPay : 0;
        dailyPayByDate.insert(dailyPay.date, dailyPay);
    }

    QVector<TeacherDailyPayData> result;

    for (QMap<QDate, TeacherDailyPayData>::const_iterator it = dailyPayByDate.cbegin();
         it != dailyPayByDate.cend();
         ++it)
    {
        result.append(it.value());
    }

    return result;
}

bool MainWindow::editSalaryDailyPays(
    const QString &teacherName,
    const QDate &month,
    QVector<TeacherDailyPayData> *dailyPays)
{
    if (dailyPays == nullptr)
    {
        return false;
    }

    QVector<TeacherDailyPayData> editedPays = salaryDailyPayDefaults(teacherName, month);

    QDialog dialog(this);
    dialog.setWindowTitle("日別の業務給・交通費");
    dialog.resize(720, 640);

    QVBoxLayout dialogLayout(&dialog);
    QLabel description(
        QString("%1 / %2年%3月").arg(teacherName).arg(month.year()).arg(month.month()),
        &dialog);
    dialogLayout.addWidget(&description);

    QTableWidget table(editedPays.size(), 5, &dialog);
    table.setHorizontalHeaderLabels({"日付", "授業数", "高校生人数", "業務給", "交通費"});
    table.verticalHeader()->setVisible(false);
    table.setAlternatingRowColors(true);
    table.horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table.horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table.horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table.horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table.horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    const QLocale japanese(QLocale::Japanese, QLocale::Japan);

    for (int row = 0; row < editedPays.size(); ++row)
    {
        const TeacherDailyPayData &dailyPay = editedPays[row];
        auto *dateItem = new QTableWidgetItem(
            japanese.toString(dailyPay.date, "M/d (ddd)"));
        auto *lessonCountItem =
            new QTableWidgetItem(QString::number(dailyPay.lessonCount));
        dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable);
        lessonCountItem->setFlags(lessonCountItem->flags() & ~Qt::ItemIsEditable);
        table.setItem(row, 0, dateItem);
        table.setItem(row, 1, lessonCountItem);

        auto *highSchoolInput = new QSpinBox(&table);
        highSchoolInput->setRange(0, 99);
        highSchoolInput->setValue(dailyPay.highSchoolStudentCount);
        table.setCellWidget(row, 2, highSchoolInput);

        auto *businessPayInput = new QSpinBox(&table);
        businessPayInput->setRange(0, 999999);
        businessPayInput->setSuffix(" 円");
        businessPayInput->setValue(dailyPay.businessPay);
        table.setCellWidget(row, 3, businessPayInput);

        auto *transportPayInput = new QSpinBox(&table);
        transportPayInput->setRange(0, 999999);
        transportPayInput->setSuffix(" 円");
        transportPayInput->setValue(dailyPay.transportPay);
        table.setCellWidget(row, 4, transportPayInput);
    }

    dialogLayout.addWidget(&table);

    QDialogButtonBox buttonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    dialogLayout.addWidget(&buttonBox);

    connect(
        &buttonBox,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);
    connect(
        &buttonBox,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    for (int row = 0; row < editedPays.size(); ++row)
    {
        auto *highSchoolInput =
            qobject_cast<QSpinBox *>(table.cellWidget(row, 2));
        auto *businessPayInput =
            qobject_cast<QSpinBox *>(table.cellWidget(row, 3));
        auto *transportPayInput =
            qobject_cast<QSpinBox *>(table.cellWidget(row, 4));

        if (highSchoolInput != nullptr)
        {
            editedPays[row].highSchoolStudentCount = highSchoolInput->value();
        }

        if (businessPayInput != nullptr)
        {
            editedPays[row].businessPay = businessPayInput->value();
        }

        if (transportPayInput != nullptr)
        {
            editedPays[row].transportPay = transportPayInput->value();
        }
    }

    *dailyPays = editedPays;
    return true;
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

    QDialog optionDialog(this);
    optionDialog.setWindowTitle("給与明細書");

    QFormLayout formLayout(&optionDialog);

    QComboBox teacherComboBox(&optionDialog);
    teacherComboBox.addItems(teacherNames);

    const QString currentTeacher = ui->teacherComboBox->currentText().trimmed();
    const int currentIndex = teacherComboBox.findText(currentTeacher);

    if (currentIndex >= 0)
    {
        teacherComboBox.setCurrentIndex(currentIndex);
    }

    QLineEdit monthInput(QDate::currentDate().toString("yyyy-MM"), &optionDialog);
    QStringList deductionHeaders = {
        "住民税", "所得税", "厚生年金", "健康保険", "雇用保険"};
    QVector<QSpinBox *> deductionInputs;

    formLayout.addRow("講師", &teacherComboBox);
    formLayout.addRow("対象月", &monthInput);

    for (const QString &header : deductionHeaders)
    {
        auto *spinBox = new QSpinBox(&optionDialog);
        spinBox->setRange(0, 999999);
        spinBox->setSuffix(" 円");
        spinBox->setValue(0);
        deductionInputs.append(spinBox);
        formLayout.addRow(header, spinBox);
    }

    QDialogButtonBox buttonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &optionDialog);
    formLayout.addRow(&buttonBox);

    connect(
        &buttonBox,
        &QDialogButtonBox::accepted,
        &optionDialog,
        &QDialog::accept);
    connect(
        &buttonBox,
        &QDialogButtonBox::rejected,
        &optionDialog,
        &QDialog::reject);

    if (optionDialog.exec() != QDialog::Accepted)
    {
        return;
    }

    const QString teacherName = teacherComboBox.currentText().trimmed();
    const QString monthText = monthInput.text().trimmed();

    if (teacherName.isEmpty() || monthText.isEmpty())
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

    QVector<int> deductions;

    for (const QSpinBox *spinBox : deductionInputs)
    {
        deductions.append(spinBox->value());
    }

    QVector<TeacherDailyPayData> dailyPays;

    if (!editSalaryDailyPays(teacherName, month, &dailyPays))
    {
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
        [this, teacherName, month, deductions, dailyPays](QPrinter *previewPrinter)
        {
            renderSalaryStatementForPrint(
                previewPrinter,
                teacherName,
                month,
                deductions,
                dailyPays);
        });

    preview.exec();
}

void MainWindow::showGuidanceReportPrintPreview()
{
    QString grade;
    QString studentName;
    QString subjectName;
    QStringList materialNames;

    if (!selectStudentSubject(
            &grade,
            &studentName,
            &subjectName,
            &materialNames,
            "指導報告書",
            true,
            true,
            true))
    {
        return;
    }

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
        [this, grade, studentName, subjectName, materialNames](QPrinter *previewPrinter)
        {
            renderGuidanceReportFormatForPrint(
                previewPrinter,
                grade,
                studentName,
                subjectName,
                materialNames);
        });

    preview.exec();
}

void MainWindow::copyStudentScheduleToClipboard()
{
    updateCell();

    QString grade;
    QString studentName;
    QString subjectName;

    if (!selectStudentSubject(
            &grade,
            &studentName,
            &subjectName,
            nullptr,
            "生徒予定表",
            false,
            false,
            false))
    {
        return;
    }

    const QString text =
        studentScheduleText(grade, studentName, subjectName);

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

        const QString requestedName = studentName.trimmed();

        for (const StudentData &candidate : gradeStudents.students)
        {
            if (candidate.Name.trimmed() == requestedName)
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

bool MainWindow::findTeacherData(
    const QString &teacherName,
    TeacherData *teacher) const
{
    for (const TeacherData &candidate : teachers)
    {
        if (candidate.name.trimmed() == teacherName.trimmed())
        {
            if (teacher != nullptr)
            {
                *teacher = candidate;
            }

            return true;
        }
    }

    return false;
}

bool MainWindow::selectStudentSubject(
    QString *grade,
    QString *studentName,
    QString *subjectName,
    QStringList *materialNames,
    const QString &title,
    bool requireSubject,
    bool includeMaterial,
    bool allowBlankSelection)
{
    QStringList gradeNames;

    for (const GradeStudents &gradeStudents : allStudents)
    {
        if (!gradeStudents.students.isEmpty())
        {
            gradeNames.append(gradeStudents.Grade);
        }
    }

    if (gradeNames.isEmpty() && !allowBlankSelection)
    {
        QMessageBox::information(this, title, "生徒が登録されていません。");
        return false;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(title);

    QFormLayout formLayout(&dialog);
    QComboBox gradeComboBox(&dialog);
    QComboBox studentComboBox(&dialog);
    QComboBox subjectComboBox(&dialog);
    QListWidget materialListWidget(&dialog);

    if (allowBlankSelection)
    {
        gradeComboBox.addItem("");
    }

    gradeComboBox.addItems(gradeNames);

    auto updateStudentNames = [&]()
    {
        const QString currentStudent = studentComboBox.currentText();

        studentComboBox.clear();

        if (allowBlankSelection)
        {
            studentComboBox.addItem("");
        }

        if (gradeComboBox.currentText().trimmed().isEmpty())
        {
            return;
        }

        for (const GradeStudents &gradeStudents : allStudents)
        {
            if (gradeStudents.Grade != gradeComboBox.currentText())
            {
                continue;
            }

            for (const StudentData &student : gradeStudents.students)
            {
                const QString name = student.Name.trimmed();

                if (!name.isEmpty())
                {
                    studentComboBox.addItem(name);
                }
            }

            break;
        }

        const int index = studentComboBox.findText(currentStudent);

        if (index >= 0)
        {
            studentComboBox.setCurrentIndex(index);
        }
    };

    auto updateMaterialNames = [&]()
    {
        materialListWidget.clear();

        for (const QString &material :
             materialNamesForStudentSubject(
                 gradeComboBox.currentText(),
                 studentComboBox.currentText(),
                 subjectComboBox.currentText()))
        {
            const QString materialName = material.trimmed();

            if (materialName.isEmpty())
            {
                continue;
            }

            auto *item = new QListWidgetItem(materialName, &materialListWidget);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
        }

        if (materialListWidget.count() == 0)
        {
            auto *item = new QListWidgetItem("登録教材なし", &materialListWidget);
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    };

    auto updateSubjectNames = [&]()
    {
        const QString currentSubject = subjectComboBox.currentText();

        subjectComboBox.clear();

        if (allowBlankSelection || !requireSubject)
        {
            subjectComboBox.addItem("");
        }

        if (studentComboBox.currentText().trimmed().isEmpty())
        {
            updateMaterialNames();
            return;
        }

        for (const QString &subject :
             subjectNamesForStudent(
                 gradeComboBox.currentText(),
                 studentComboBox.currentText()))
        {
            const QString subjectText = subject.trimmed();

            if (!subjectText.isEmpty() &&
                subjectComboBox.findText(subjectText) < 0)
            {
                subjectComboBox.addItem(subjectText);
            }
        }

        const int index = subjectComboBox.findText(currentSubject);

        if (index >= 0)
        {
            subjectComboBox.setCurrentIndex(index);
        }

        updateMaterialNames();
    };

    updateStudentNames();
    updateSubjectNames();

    connect(
        &gradeComboBox,
        &QComboBox::currentTextChanged,
        &dialog,
        [&](const QString &)
        {
            updateStudentNames();
            updateSubjectNames();
        });
    connect(
        &studentComboBox,
        &QComboBox::currentTextChanged,
        &dialog,
        [&](const QString &)
        {
            updateSubjectNames();
        });
    connect(
        &subjectComboBox,
        &QComboBox::currentTextChanged,
        &dialog,
        [&](const QString &)
        {
            updateMaterialNames();
        });

    formLayout.addRow("学年", &gradeComboBox);
    formLayout.addRow("生徒名", &studentComboBox);
    formLayout.addRow("教科", &subjectComboBox);

    if (includeMaterial)
    {
        materialListWidget.setMinimumHeight(110);
        formLayout.addRow("教材", &materialListWidget);
    }

    QDialogButtonBox buttonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog);
    formLayout.addRow(&buttonBox);

    connect(
        &buttonBox,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);
    connect(
        &buttonBox,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    if (studentComboBox.currentText().trimmed().isEmpty() && !allowBlankSelection)
    {
        QMessageBox::information(this, title, "生徒を選択してください。");
        return false;
    }

    if (requireSubject &&
        subjectComboBox.currentText().trimmed().isEmpty() &&
        !allowBlankSelection)
    {
        QMessageBox::information(this, title, "教科を選択してください。");
        return false;
    }

    if (grade != nullptr)
    {
        *grade = gradeComboBox.currentText().trimmed();
    }

    if (studentName != nullptr)
    {
        *studentName = studentComboBox.currentText().trimmed();
    }

    if (subjectName != nullptr)
    {
        *subjectName = subjectComboBox.currentText().trimmed();
    }

    if (materialNames != nullptr)
    {
        materialNames->clear();

        if (includeMaterial)
        {
            for (int i = 0; i < materialListWidget.count(); ++i)
            {
                const QListWidgetItem *item = materialListWidget.item(i);

                if (item != nullptr && item->checkState() == Qt::Checked)
                {
                    materialNames->append(item->text().trimmed());
                }
            }
        }
    }

    return true;
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
    const QString &studentName,
    const QString &subjectName) const
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

        if (!subjectName.trimmed().isEmpty() &&
            entry.subject != subjectName)
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
    const QDate &month,
    const QVector<int> &deductions,
    const QVector<TeacherDailyPayData> &dailyPays)
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
    TeacherData teacher;
    teacher.name = teacherName;
    teacher.oneOnTwoRate = defaultSalaryOneOnTwoRate;
    teacher.oneOnOneRate = defaultSalaryOneOnOneRate;
    teacher.transportPay = defaultSalaryTransportPay;
    teacher.highSchoolAllowance = defaultSalaryHighSchoolAllowance;
    findTeacherData(teacherName, &teacher);
    const int oneOnTwoRate = teacher.oneOnTwoRate;
    const int oneOnOneRate = teacher.oneOnOneRate;
    const int transportPay = teacher.transportPay;
    const int highSchoolAllowance = teacher.highSchoolAllowance;
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
    QMap<QDate, TeacherDailyPayData> dailyPayByDate;

    for (const TeacherDailyPayData &dailyPay : dailyPays)
    {
        if (dailyPay.teacherName.trimmed() == teacherName.trimmed() &&
            dailyPay.date >= monthStart &&
            dailyPay.date <= monthEnd)
        {
            dailyPayByDate.insert(dailyPay.date, dailyPay);
        }
    }

    for (QMap<QDate, SalaryDaySummary>::iterator it = summaries.begin();
         it != summaries.end();
         ++it)
    {
        SalaryDaySummary &summary = it.value();

        if (dailyPayByDate.contains(it.key()))
        {
            const TeacherDailyPayData dailyPay = dailyPayByDate.value(it.key());
            summary.highSchoolAllowanceCount = dailyPay.highSchoolStudentCount;
            summary.businessPay = dailyPay.businessPay;
            summary.transportPay = dailyPay.transportPay;
        }
        else if (summary.oneOnTwoCount > 0 ||
                 summary.oneOnOneCount > 0 ||
                 summary.highSchoolAllowanceCount > 0)
        {
            summary.businessPay = 0;
            summary.transportPay = transportPay;
        }

        grandTotal += summary.total(
            oneOnTwoRate,
            oneOnOneRate,
            highSchoolAllowance);
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
            oneOnTwoRate,
            oneOnOneRate,
            highSchoolAllowance);
        QStringList row = {
            currentDay.toString("M月d日"),
            summary.oneOnTwoCount > 0 ? QString::number(summary.oneOnTwoCount) : QString(),
            summary.oneOnTwoCount > 0 ? QString::number(oneOnTwoRate) : QString(),
            summary.oneOnOneCount > 0 ? QString::number(summary.oneOnOneCount) : QString(),
            summary.oneOnOneCount > 0 ? QString::number(oneOnOneRate) : QString(),
            summary.highSchoolAllowanceCount > 0
                ? QString::number(summary.highSchoolAllowanceCount * highSchoolAllowance)
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
    int deductionTotal = 0;

    for (int i = 0; i < deductionHeaders.size(); ++i)
    {
        const int deduction =
            i < deductions.size() ? qMax(0, deductions[i]) : 0;
        deductionTotal += deduction;

        drawBoxText(QRectF(x, y + 26 * scale, 96 * scale, rowHeight), deductionHeaders[i], Qt::AlignCenter);
        drawBoxText(QRectF(x, y + 26 * scale + rowHeight, 96 * scale, rowHeight), moneyText(deduction), Qt::AlignCenter);
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
        deductionTotal > 0
            ? QString("￥ %1").arg(moneyText(deductionTotal))
            : QString("￥ -"),
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
        QString("￥ %1").arg(moneyText(grandTotal - deductionTotal)),
        Qt::AlignRight | Qt::AlignVCenter,
        true);
}

void MainWindow::renderGuidanceReportFormatForPrint(
    QPrinter *printer,
    const QString &grade,
    const QString &studentName,
    const QString &subjectName,
    const QStringList &materialNames)
{
    QPainter painter(printer);

    if (!painter.isActive())
    {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRectF pageRect =
        printer->pageLayout().paintRectPixels(printer->resolution());
    const qreal scale = qMax(1.0, printer->resolution() / 96.0);
    const QRectF area = pageRect.adjusted(
        38 * scale,
        32 * scale,
        -38 * scale,
        -32 * scale);

    const QPen outerPen(Qt::black, 2);
    const QPen normalPen(Qt::black, 1);
    const QPen lightPen(QColor(205, 205, 205), 1);

    auto setFont = [&painter](bool bold, int pointSize)
    {
        QFont font = painter.font();
        font.setBold(bold);
        font.setPointSize(pointSize);
        painter.setFont(font);
    };

    auto drawText = [&](const QRectF &rect, const QString &text, int alignment, bool bold = false, int pointSize = 9)
    {
        const QFont previousFont = painter.font();
        const QPen previousPen = painter.pen();
        setFont(bold, pointSize);
        painter.setPen(Qt::black);
        painter.drawText(rect.adjusted(3 * scale, 1 * scale, -3 * scale, -1 * scale), alignment | Qt::TextWordWrap, text);
        painter.setFont(previousFont);
        painter.setPen(previousPen);
    };

    auto drawBox = [&](const QRectF &rect, const QString &text = QString(), int alignment = Qt::AlignLeft | Qt::AlignVCenter, bool bold = false)
    {
        painter.setPen(normalPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);

        if (!text.isEmpty())
        {
            drawText(rect, text, alignment, bold);
        }
    };

    auto drawCross = [&](const QRectF &rect)
    {
        painter.save();
        painter.setPen(lightPen);
        painter.drawLine(QPointF(rect.center().x(), rect.top()), QPointF(rect.center().x(), rect.bottom()));
        painter.drawLine(QPointF(rect.left(), rect.center().y()), QPointF(rect.right(), rect.center().y()));
        painter.restore();
    };

    auto drawHeaderRow = [&](const QRectF &rect, const QStringList &headers, const QVector<qreal> &weights)
    {
        const qreal totalWeight = std::accumulate(weights.cbegin(), weights.cend(), 0.0);
        qreal x = rect.left();

        for (int i = 0; i < headers.size(); ++i)
        {
            const qreal width =
                i == headers.size() - 1
                    ? rect.right() - x
                    : rect.width() * weights.value(i, 1.0) / totalWeight;
            drawBox(QRectF(x, rect.top(), width, rect.height()), headers[i], Qt::AlignCenter, true);
            x += width;
        }
    };

    auto drawTableRow = [&](const QRectF &rect, const QStringList &texts, const QVector<qreal> &weights, bool bold, bool cross)
    {
        const qreal totalWeight = std::accumulate(weights.cbegin(), weights.cend(), 0.0);
        qreal x = rect.left();

        for (int i = 0; i < texts.size(); ++i)
        {
            const qreal width =
                i == texts.size() - 1
                    ? rect.right() - x
                    : rect.width() * weights.value(i, 1.0) / totalWeight;
            const QRectF cell(x, rect.top(), width, rect.height());

            if (cross)
            {
                drawCross(cell);
            }

            drawBox(cell, texts[i], i == 0 ? Qt::AlignLeft | Qt::AlignVCenter : Qt::AlignCenter, bold);
            x += width;
        }
    };

    auto drawInputBlock = [&](const QRectF &blockRect)
    {
        painter.setPen(outerPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(blockRect);

        const qreal padding = 7 * scale;
        const QRectF inner = blockRect.adjusted(padding, padding, -padding, -padding);
        const qreal topRowHeight = 25 * scale;
        const qreal nextRowHeight = 25 * scale;
        const qreal top = inner.top();
        const qreal bottom = inner.bottom();
        const QRectF topRow(inner.left(), top, inner.width(), topRowHeight);

        const QVector<qreal> topWeights = {2.2, 2.2, 2.4, 2.2};
        const QStringList topTexts = {
            "   月   日（   ）",
            "   ：   ～   ：   ",
            "出席・遅刻（   ）分",
            "講師名：      "};
        drawTableRow(topRow, topTexts, topWeights, false, false);

        const qreal bodyTop = topRow.bottom();
        const qreal nextTop = bottom - nextRowHeight;
        const QRectF body(inner.left(), bodyTop, inner.width(), nextTop - bodyTop);
        const qreal halfWidth = body.width() / 2.0;
        const QRectF leftBox(body.left(), body.top(), halfWidth, body.height());
        const QRectF rightBox(leftBox.right(), body.top(), body.right() - leftBox.right(), body.height());

        painter.setPen(normalPen);
        painter.drawRect(leftBox);
        painter.drawRect(rightBox);

        const qreal sidePadding = 5 * scale;
        const QRectF left = leftBox.adjusted(sidePadding, sidePadding, -sidePadding, -sidePadding);
        const QRectF right = rightBox.adjusted(sidePadding, sidePadding, -sidePadding, -sidePadding);

        qreal y = left.top();
        const qreal unitHeight = 23 * scale;
        drawBox(QRectF(left.left(), y, left.width(), unitHeight), "単元名：        ");
        y += unitHeight;

        const qreal materialTableHeight = 96 * scale;
        const qreal materialRowHeight = materialTableHeight / 4.0;
        const QVector<qreal> materialWeights = {1.0, 1.0};
        drawHeaderRow(
            QRectF(left.left(), y, left.width(), materialRowHeight),
            {"教材名", "ページ・問題番号"},
            materialWeights);
        y += materialRowHeight;

        for (int i = 0; i < 3; ++i)
        {
            drawTableRow(
                QRectF(left.left(), y, left.width(), materialRowHeight),
                {materialNames.value(i), ""},
                materialWeights,
                false,
                false);
            y += materialRowHeight;
        }

        const qreal sectionLabelHeight = 20 * scale;
        drawText(
            QRectF(left.left(), y, left.width(), sectionLabelHeight),
            "宿題実施状況",
            Qt::AlignLeft | Qt::AlignVCenter,
            true,
            9);
        y += sectionLabelHeight;

        const QRectF homeworkStatusRect(left.left(), y, left.width(), left.bottom() - y);
        const qreal homeworkStatusRowHeight = homeworkStatusRect.height() / 7.0;
        const QVector<qreal> homeworkStatusWeights = {3.0, 7.0};
        const QStringList homeworkLabels = {
            "宿題項目",
            "",
            "",
            "テスト",
            "問題正答率",
            "理解度",
            "集中度"};
        const QStringList homeworkValues = {
            "評価",
            "１・２・３・４・５",
            "１・２・３・４・５",
            "得点：      /      内容：",
            "１・２・３・４・５",
            "１・２・３・４・５",
            "１・２・３・４・５"};

        for (int i = 0; i < homeworkLabels.size(); ++i)
        {
            drawTableRow(
                QRectF(homeworkStatusRect.left(), homeworkStatusRect.top() + homeworkStatusRowHeight * i, homeworkStatusRect.width(), homeworkStatusRowHeight),
                {homeworkLabels[i], homeworkValues[i]},
                homeworkStatusWeights,
                i == 0,
                false);
        }

        y = right.top();
        drawText(
            QRectF(right.left(), y, right.width(), sectionLabelHeight),
            "宿題",
            Qt::AlignLeft | Qt::AlignVCenter,
            true,
            9);
        y += sectionLabelHeight;

        const qreal assignmentTableHeight = qMin(210 * scale, right.height() * 0.58);
        const qreal assignmentRowHeight = assignmentTableHeight / 9.0;
        const QVector<qreal> assignmentWeights = {2.0, 4.0, 6.0};
        drawHeaderRow(
            QRectF(right.left(), y, right.width(), assignmentRowHeight),
            {"日付", "教材名", "ページ・問題番号"},
            assignmentWeights);
        y += assignmentRowHeight;

        for (int i = 0; i < 8; ++i)
        {
            drawTableRow(
                QRectF(right.left(), y, right.width(), assignmentRowHeight),
                {"", "", ""},
                assignmentWeights,
                false,
                true);
            y += assignmentRowHeight;
        }

        drawText(
            QRectF(right.left(), y, right.width(), sectionLabelHeight),
            "ご家庭への連絡",
            Qt::AlignLeft | Qt::AlignVCenter,
            true,
            9);
        y += sectionLabelHeight;

        const QRectF messageRect(right.left(), y, right.width(), right.bottom() - y);
        const qreal messageRowHeight = messageRect.height() / 4.0;

        for (int i = 0; i < 4; ++i)
        {
            const QRectF row(messageRect.left(), messageRect.top() + messageRowHeight * i, messageRect.width(), messageRowHeight);
            drawCross(row);
            drawBox(row);
        }

        const QRectF nextRow(inner.left(), nextTop, inner.width(), nextRowHeight);
        drawBox(nextRow, "次回予定：   月    日    ：   ～       教科：    ");
    };

    const qreal titleHeight = 28 * scale;
    const qreal infoHeight = 28 * scale;
    const qreal headerGap = 6 * scale;
    const qreal blockGap = 10 * scale;

    drawText(
        QRectF(area.left(), area.top(), area.width(), titleHeight),
        "指導報告書",
        Qt::AlignCenter,
        true,
        15);

    drawText(
        QRectF(area.left(), area.top() + titleHeight, area.width(), infoHeight),
        QString("学年　%1　　　氏名　%2　　　教科　%3")
            .arg(grade)
            .arg(studentName)
            .arg(subjectName),
        Qt::AlignCenter,
        true,
        12);

    const qreal blockTop = area.top() + titleHeight + infoHeight + headerGap;
    const qreal blockHeight = (area.bottom() - blockTop - blockGap) / 2.0;

    drawInputBlock(QRectF(area.left(), blockTop, area.width(), blockHeight));
    drawInputBlock(QRectF(area.left(), blockTop + blockHeight + blockGap, area.width(), blockHeight));
}

#if 0
void MainWindow::renderGuidanceReportFormatForPrint(
    QPrinter *printer,
    const QString &grade,
    const QString &studentName,
    const QString &subjectName,
    const QString &materialName)
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
    const qreal headerHeight = 78 * scale;
    const qreal gap = 22 * scale;
    const qreal reportTop = area.top() + headerHeight;
    const qreal reportHeight =
        (area.bottom() - reportTop - gap) / 2.0;

    auto drawReport = [this, &painter, scale, materialName](const QRectF &rect)
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

        qreal y = rect.top() + 14 * scale;

#if 0
        drawSchedulePrintText(
            &painter,
            QRectF(rect.left(), rect.top() + 5 * scale, rect.width(), 24 * scale),
            "指導報告書",
            Qt::AlignCenter,
            true);

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
#endif

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

            if (i == 0 && !materialName.trimmed().isEmpty())
            {
                drawSchedulePrintText(
                    &painter,
                    materialBox.adjusted(5 * scale, 0, -5 * scale, 0),
                    materialName,
                    Qt::AlignLeft | Qt::AlignVCenter);
            }

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

    drawSchedulePrintText(
        &painter,
        QRectF(area.left(), area.top(), area.width(), 30 * scale),
        "指導報告書",
        Qt::AlignCenter,
        true);

    const QRectF infoBox(
        area.left(),
        area.top() + 36 * scale,
        area.width(),
        30 * scale);
    const qreal infoWidth = infoBox.width() / 3.0;

    painter.setPen(QPen(Qt::black, 2));
    painter.drawRect(infoBox);
    painter.drawLine(
        QPointF(infoBox.left() + infoWidth, infoBox.top()),
        QPointF(infoBox.left() + infoWidth, infoBox.bottom()));
    painter.drawLine(
        QPointF(infoBox.left() + infoWidth * 2.0, infoBox.top()),
        QPointF(infoBox.left() + infoWidth * 2.0, infoBox.bottom()));

    drawSchedulePrintText(
        &painter,
        QRectF(infoBox.left(), infoBox.top(), infoWidth, infoBox.height()),
        QString("学年　%1").arg(grade),
        Qt::AlignCenter,
        true);
    drawSchedulePrintText(
        &painter,
        QRectF(infoBox.left() + infoWidth, infoBox.top(), infoWidth, infoBox.height()),
        QString("氏名　%1").arg(studentName),
        Qt::AlignCenter,
        true);
    drawSchedulePrintText(
        &painter,
        QRectF(infoBox.left() + infoWidth * 2.0, infoBox.top(), infoWidth, infoBox.height()),
        QString("教科　%1").arg(subjectName),
        Qt::AlignCenter,
        true);

    drawReport(QRectF(area.left(), reportTop, area.width(), reportHeight));
    drawReport(QRectF(area.left(), reportTop + reportHeight + gap, area.width(), reportHeight));
}
#endif

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
