#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCoreApplication>
#include <QDateEdit>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>

#include <algorithm>

// 指導報告書PDFタブの表示と操作を初期化する
void MainWindow::setupGuidanceReportPdfTab()
{
    guidanceReportPdfDocument = new QPdfDocument(this);
    ui->guidanceReportPdfView->setDocument(guidanceReportPdfDocument);
    ui->guidanceReportPdfView->setPageMode(QPdfView::PageMode::SinglePage);
    ui->guidanceReportPdfView->setZoomMode(QPdfView::ZoomMode::FitInView);

    const int teacherRowHeight = ui->guidanceReportPdfTeacherList->fontMetrics().height() + 6;
    ui->guidanceReportPdfTeacherList->setFixedHeight(
        teacherRowHeight * 5 + ui->guidanceReportPdfTeacherList->frameWidth() * 2);
    ui->guidanceReportPdfDateEdit->setDate(QDate::currentDate());
    ui->guidanceReportPdfPreviousButton->setEnabled(false);
    ui->guidanceReportPdfNextButton->setEnabled(false);

    connect(
        ui->guidanceReportPdfDateEdit,
        &QDateEdit::dateChanged,
        this,
        [this](const QDate &)
        {
            ui->guidanceReportPdfTeacherList->clearSelection();
            ui->guidanceReportPdfTeacherList->setCurrentItem(nullptr);
            guidanceReportPdfEntries.clear();

            if (guidanceReportPdfDocument != nullptr &&
                guidanceReportPdfDocument->pageCount() > 0)
            {
                guidanceReportPdfEntries.resize(
                    guidanceReportPdfDocument->pageCount());
                showGuidanceReportPdfPage(0);
            }
            else
            {
                ui->guidanceReportPdfStudentEdit->clear();
                ui->guidanceReportPdfSubjectEdit->clear();
            }

            refreshGuidanceReportTeacherList();
        });
    connect(
        ui->guidanceReportPdfTeacherList,
        &QListWidget::itemSelectionChanged,
        this,
        &MainWindow::loadGuidanceReportEntriesForSelectedTeacher);
    connect(
        ui->guidanceReportPdfLatestButton,
        &QPushButton::clicked,
        this,
        &MainWindow::loadLatestGuidanceReportPdf);
    connect(
        ui->guidanceReportPdfOpenButton,
        &QPushButton::clicked,
        this,
        &MainWindow::selectGuidanceReportPdf);
    connect(
        ui->guidanceReportPdfPreviousButton,
        &QPushButton::clicked,
        this,
        &MainWindow::showPreviousGuidanceReportPdfPage);
    connect(
        ui->guidanceReportPdfNextButton,
        &QPushButton::clicked,
        this,
        &MainWindow::advanceGuidanceReportPdfPage);
    connect(
        ui->guidanceReportPdfStudentEdit,
        &QLineEdit::returnPressed,
        ui->guidanceReportPdfNextButton,
        &QPushButton::click);
    connect(
        ui->guidanceReportPdfSubjectEdit,
        &QLineEdit::returnPressed,
        ui->guidanceReportPdfNextButton,
        &QPushButton::click);
    connect(
        ui->mainTabWidget,
        &QTabWidget::currentChanged,
        this,
        [this](int index)
        {
            if (ui->mainTabWidget->widget(index) == ui->guidanceReportPdfTab)
            {
                activateGuidanceReportPdfTab();
            }
        });

    refreshGuidanceReportTeacherList();
}

// 指導報告書PDFタブを最新の時間割内容へ更新する
void MainWindow::activateGuidanceReportPdfTab()
{
    updateCell();
    refreshGuidanceReportTeacherList();
}

// 分割前の指導報告書PDFを探すフォルダを設定する
void MainWindow::selectGuidanceReportPdfDirectory()
{
    const QString selectedDirectory = QFileDialog::getExistingDirectory(
        this,
        "指導報告書の場所を選択",
        guidanceReportPdfDir);

    if (selectedDirectory.isEmpty())
    {
        return;
    }

    QJsonObject root = loadMasterJson();
    root["guidanceReportPdfDir"] = QDir::fromNativeSeparators(selectedDirectory);

    if (!saveMasterJson(root))
    {
        return;
    }

    guidanceReportPdfDir = QDir::fromNativeSeparators(selectedDirectory);
    statusBar()->showMessage(
        QString("分割前の指導報告書の場所を %1 に設定しました").arg(guidanceReportPdfDir),
        3000);
}

// 分割後の指導報告書PDFを保存するフォルダを設定する
void MainWindow::selectGuidanceReportPdfOutputDirectory()
{
    const QString selectedDirectory = QFileDialog::getExistingDirectory(
        this,
        "分割後の指導報告書の保存先を選択",
        guidanceReportPdfOutputDir);

    if (selectedDirectory.isEmpty())
    {
        return;
    }

    QJsonObject root = loadMasterJson();
    root["guidanceReportPdfOutputDir"] =
        QDir::fromNativeSeparators(selectedDirectory);

    if (!saveMasterJson(root))
    {
        return;
    }

    guidanceReportPdfOutputDir = QDir::fromNativeSeparators(selectedDirectory);
    statusBar()->showMessage(
        QString("分割後の指導報告書の保存先を %1 に設定しました")
            .arg(guidanceReportPdfOutputDir),
        3000);
}

// 指定日の授業を指導報告書の入力順で取得する
QVector<LessonRecord> MainWindow::guidanceReportLessonsForDate(
    const QDate &date) const
{
    if (!date.isValid())
    {
        return {};
    }

    const QDate targetMonday = mondayOf(date);
    QVector<QVector<TeacherColumn>> loadedSchedule;
    QDate loadedMonday;
    QStringList loadedDays = days;
    QStringList loadedPeriods = periods;

    if (targetMonday == scheduleMonday)
    {
        loadedSchedule = schedule;
        loadedMonday = scheduleMonday;
    }
    else if (!loadScheduleDataFromFile(
                 targetMonday,
                 &loadedMonday,
                 &loadedSchedule,
                 &loadedDays,
                 &loadedPeriods))
    {
        return {};
    }

    QVector<LessonRecord> result;

    for (const LessonRecord &entry :
         scheduleEntriesFor(
             loadedMonday,
             loadedSchedule,
             loadedDays,
             loadedPeriods))
    {
        if (entry.date == date)
        {
            result.append(entry);
        }
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const LessonRecord &a, const LessonRecord &b)
        {
            if (a.teacherIndex != b.teacherIndex)
            {
                return a.teacherIndex < b.teacherIndex;
            }

            if (a.periodIndex != b.periodIndex)
            {
                return a.periodIndex < b.periodIndex;
            }

            return a.studentIndex < b.studentIndex;
        });
    return result;
}

// 選択日に授業がある講師の一覧を更新する
void MainWindow::refreshGuidanceReportTeacherList()
{
    const QString selectedTeacher =
        ui->guidanceReportPdfTeacherList->currentItem() != nullptr
            ? ui->guidanceReportPdfTeacherList->currentItem()->text()
            : QString();
    const QVector<LessonRecord> entries = guidanceReportLessonsForDate(
        ui->guidanceReportPdfDateEdit->date());
    QSet<QString> addedTeachers;
    bool restoredSelection = false;

    {
        const QSignalBlocker signalBlocker(ui->guidanceReportPdfTeacherList);
        ui->guidanceReportPdfTeacherList->clear();

        for (const LessonRecord &entry : entries)
        {
            const QString teacherName = entry.teacherName.trimmed();

            if (teacherName.isEmpty() || addedTeachers.contains(teacherName))
            {
                continue;
            }

            addedTeachers.insert(teacherName);
            ui->guidanceReportPdfTeacherList->addItem(teacherName);
        }

        if (!selectedTeacher.isEmpty())
        {
            const QList<QListWidgetItem *> matches =
                ui->guidanceReportPdfTeacherList->findItems(
                    selectedTeacher,
                    Qt::MatchExactly);

            if (!matches.isEmpty())
            {
                ui->guidanceReportPdfTeacherList->setCurrentItem(matches.first());
                restoredSelection = true;
            }
        }
    }

    if (!selectedTeacher.isEmpty() && !restoredSelection)
    {
        loadGuidanceReportEntriesForSelectedTeacher();
    }
}

// PDFへの自動入力文字列を設定に従って整形する
QString MainWindow::normalizeGuidanceReportPdfAutoInputText(const QString &text) const
{
    QString normalizedText = text.trimmed();

    if (guidanceReportPdfRemoveSpacesFromAutoInput != 0)
    {
        normalizedText.remove(QLatin1Char(' '));
        normalizedText.remove(QChar(0x3000));
    }

    return normalizedText;
}

// 選択講師の授業を各PDFページの入力欄へ割り当てる
void MainWindow::loadGuidanceReportEntriesForSelectedTeacher()
{
    const QListWidgetItem *selectedItem =
        ui->guidanceReportPdfTeacherList->currentItem();
    QVector<GuidanceReportPdfEntry> teacherEntries;

    if (selectedItem != nullptr)
    {
        const QString teacherName = selectedItem->text().trimmed();

        for (const LessonRecord &lesson : guidanceReportLessonsForDate(
                 ui->guidanceReportPdfDateEdit->date()))
        {
            if (lesson.teacherName.trimmed() != teacherName)
            {
                continue;
            }

            GuidanceReportPdfEntry entry;
            entry.studentName = normalizeGuidanceReportPdfAutoInputText(
                studentNameWithHonorific(
                    lesson.studentGrade,
                    lesson.studentName,
                    false));
            entry.subject = normalizeGuidanceReportPdfAutoInputText(
                lesson.subject);
            teacherEntries.append(entry);
        }
    }

    const int pageCount =
        guidanceReportPdfDocument != nullptr
            ? guidanceReportPdfDocument->pageCount()
            : 0;

    if (pageCount > 0)
    {
        guidanceReportPdfEntries.clear();
        guidanceReportPdfEntries.resize(pageCount);

        for (int i = 0; i < pageCount && i < teacherEntries.size(); ++i)
        {
            guidanceReportPdfEntries[i] = teacherEntries[i];
        }

        showGuidanceReportPdfPage(0);
        return;
    }

    guidanceReportPdfEntries = teacherEntries;

    if (guidanceReportPdfEntries.isEmpty())
    {
        ui->guidanceReportPdfStudentEdit->clear();
        ui->guidanceReportPdfSubjectEdit->clear();
        return;
    }

    ui->guidanceReportPdfStudentEdit->setText(
        guidanceReportPdfEntries.first().studentName);
    ui->guidanceReportPdfSubjectEdit->setText(
        guidanceReportPdfEntries.first().subject);
}

// 設定フォルダ内で更新日時が最も新しいPDFを開く
void MainWindow::loadLatestGuidanceReportPdf()
{
    QDir directory(guidanceReportPdfDir);
    directory.setNameFilters({"*.pdf"});
    directory.setFilter(QDir::Files | QDir::Readable);
    directory.setSorting(QDir::Time);

    const QFileInfoList files = directory.entryInfoList();

    if (files.isEmpty())
    {
        QMessageBox::information(
            this,
            "指導報告書",
            "設定した場所にPDFがありません。");
        return;
    }

    loadGuidanceReportPdfFile(files.first().absoluteFilePath());
}

// ファイル選択画面から指導報告書PDFを開く
void MainWindow::selectGuidanceReportPdf()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "指導報告書PDFを開く",
        guidanceReportPdfDir,
        "PDFファイル (*.pdf)");

    if (!filePath.isEmpty())
    {
        loadGuidanceReportPdfFile(filePath);
    }
}

// 指定された指導報告書PDFをプレビューへ読み込む
void MainWindow::loadGuidanceReportPdfFile(const QString &filePath)
{
    if (guidanceReportPdfDocument == nullptr)
    {
        return;
    }

    guidanceReportPdfDocument->close();

    if (guidanceReportPdfDocument->load(filePath) != QPdfDocument::Error::None ||
        guidanceReportPdfDocument->pageCount() <= 0)
    {
        QMessageBox::warning(
            this,
            "PDF読み込みエラー",
            "PDFを読み込めませんでした。");
        return;
    }

    if (guidanceReportPdfDocument->pageCount() >= 100)
    {
        guidanceReportPdfDocument->close();
        guidanceReportPdfSourcePath.clear();
        guidanceReportPdfCurrentPage = -1;
        ui->guidanceReportPdfPageLabel->setText("PDFを選択してください");
        ui->guidanceReportPdfPreviousButton->setEnabled(false);
        ui->guidanceReportPdfNextButton->setEnabled(false);
        ui->guidanceReportPdfNextButton->setText("次へ進む");
        loadGuidanceReportEntriesForSelectedTeacher();
        QMessageBox::warning(
            this,
            "PDF読み込みエラー",
            "100ページ以上のPDFには対応していません。");
        return;
    }

    guidanceReportPdfSourcePath = QFileInfo(filePath).absoluteFilePath();
    guidanceReportPdfCurrentPage = -1;
    loadGuidanceReportEntriesForSelectedTeacher();
}

// 指定ページとその名前・教科の編集内容を表示する
void MainWindow::showGuidanceReportPdfPage(int pageIndex)
{
    if (guidanceReportPdfDocument == nullptr ||
        pageIndex < 0 ||
        pageIndex >= guidanceReportPdfDocument->pageCount())
    {
        return;
    }

    if (guidanceReportPdfEntries.size() < guidanceReportPdfDocument->pageCount())
    {
        guidanceReportPdfEntries.resize(guidanceReportPdfDocument->pageCount());
    }

    guidanceReportPdfCurrentPage = pageIndex;
    ui->guidanceReportPdfView->pageNavigator()->jump(
        pageIndex,
        QPointF(0, 0));
    ui->guidanceReportPdfStudentEdit->setText(
        guidanceReportPdfEntries[pageIndex].studentName);
    ui->guidanceReportPdfSubjectEdit->setText(
        guidanceReportPdfEntries[pageIndex].subject);
    ui->guidanceReportPdfPageLabel->setText(
        QString("%1 / %2　%3")
            .arg(pageIndex + 1)
            .arg(guidanceReportPdfDocument->pageCount())
            .arg(QFileInfo(guidanceReportPdfSourcePath).fileName()));
    ui->guidanceReportPdfPreviousButton->setEnabled(pageIndex > 0);
    ui->guidanceReportPdfNextButton->setEnabled(true);
    ui->guidanceReportPdfNextButton->setText(
        pageIndex == guidanceReportPdfDocument->pageCount() - 1
            ? "分割して名前を変更"
            : "次へ進む");
}

// 表示中ページの名前と教科を作業データへ保存する
void MainWindow::saveGuidanceReportPdfEditor()
{
    if (guidanceReportPdfCurrentPage < 0 ||
        guidanceReportPdfCurrentPage >= guidanceReportPdfEntries.size())
    {
        return;
    }

    guidanceReportPdfEntries[guidanceReportPdfCurrentPage].studentName =
        ui->guidanceReportPdfStudentEdit->text().trimmed();
    guidanceReportPdfEntries[guidanceReportPdfCurrentPage].subject =
        ui->guidanceReportPdfSubjectEdit->text().trimmed();
}

// 編集内容を保存して前のPDFページを表示する
void MainWindow::showPreviousGuidanceReportPdfPage()
{
    if (guidanceReportPdfCurrentPage <= 0)
    {
        return;
    }

    saveGuidanceReportPdfEditor();
    showGuidanceReportPdfPage(guidanceReportPdfCurrentPage - 1);
}

// 次ページへ進み、最終ページではPDFを分割して名前を変更する
void MainWindow::advanceGuidanceReportPdfPage()
{
    if (guidanceReportPdfDocument == nullptr ||
        guidanceReportPdfDocument->pageCount() <= 0 ||
        guidanceReportPdfCurrentPage < 0)
    {
        return;
    }

    if (ui->guidanceReportPdfStudentEdit->text().trimmed().isEmpty() ||
        ui->guidanceReportPdfSubjectEdit->text().trimmed().isEmpty())
    {
        QMessageBox::warning(
            this,
            "入力エラー",
            "名前と教科を入力してください。");
        return;
    }

    saveGuidanceReportPdfEditor();

    if (guidanceReportPdfCurrentPage < guidanceReportPdfDocument->pageCount() - 1)
    {
        showGuidanceReportPdfPage(guidanceReportPdfCurrentPage + 1);
        return;
    }

    if (!splitAndRenameGuidanceReportPdf())
    {
        return;
    }

    QMessageBox::information(
        this,
        "指導報告書",
        "PDFの分割と名前変更が完了しました。");
    resetGuidanceReportPdfWork();
}

// 既存ファイルと重複しない指導報告書PDFの保存先を返す
QString MainWindow::uniqueGuidanceReportPdfPath(
    const QString &baseName) const
{
    const QDir outputDirectory(guidanceReportPdfOutputDir);
    QString candidate = outputDirectory.filePath(baseName + ".pdf");
    int suffix = 1;

    while (QFileInfo::exists(candidate))
    {
        candidate = outputDirectory.filePath(
            QString("%1(%2).pdf").arg(baseName).arg(suffix));
        ++suffix;
    }

    return candidate;
}

// PDFファイル名に使用できない文字を置換する
QString MainWindow::sanitizeGuidanceReportPdfFileName(
    const QString &fileName) const
{
    QString result = fileName;
    result.replace(QRegularExpression("[\\\\/:*?\"<>|\\x00-\\x1f]"), "_");
    return result;
}

// 指導報告書PDFを1ページずつ分割して入力内容で名前を付ける
bool MainWindow::splitAndRenameGuidanceReportPdf()
{
    const int pageCount =
        guidanceReportPdfDocument != nullptr
            ? guidanceReportPdfDocument->pageCount()
            : 0;

    if (pageCount <= 0 || guidanceReportPdfEntries.size() < pageCount)
    {
        return false;
    }

    QDir outputDirectory(guidanceReportPdfOutputDir);

    if (!outputDirectory.exists() && !outputDirectory.mkpath("."))
    {
        QMessageBox::warning(
            this,
            "保存エラー",
            "指導報告書の保存先を作成できませんでした。");
        return false;
    }

    const QString qpdfPath = QDir(QCoreApplication::applicationDirPath()).filePath(
        "qpdf12.3.2/bin/qpdf.exe");

    if (!QFileInfo::exists(qpdfPath))
    {
        QMessageBox::warning(
            this,
            "qpdfエラー",
            QString("qpdfが見つかりません。\n%1").arg(qpdfPath));
        return false;
    }

    QTemporaryDir temporaryDirectory(
        outputDirectory.filePath(".timetable-guidance-XXXXXX"));

    if (!temporaryDirectory.isValid())
    {
        QMessageBox::warning(
            this,
            "保存エラー",
            "PDF分割用の一時フォルダを作成できませんでした。");
        return false;
    }

    const QString outputPattern =
        QDir(temporaryDirectory.path()).filePath("page-%d.pdf");
    QProcess process;
    process.start(
        qpdfPath,
        {"--split-pages", guidanceReportPdfSourcePath, outputPattern});

    if (!process.waitForStarted() ||
        !process.waitForFinished(-1) ||
        process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0)
    {
        QMessageBox::warning(
            this,
            "PDF分割エラー",
            QString::fromLocal8Bit(process.readAllStandardError()));
        return false;
    }

    QFileInfoList splitFiles;
    const int pageNumberWidth = pageCount >= 10 ? 2 : 1;

    for (int i = 0; i < pageCount; ++i)
    {
        const QString splitPath = QDir(temporaryDirectory.path()).filePath(
            QString("page-%1.pdf")
                .arg(i + 1, pageNumberWidth, 10, QChar('0')));

        if (!QFileInfo::exists(splitPath))
        {
            QMessageBox::warning(
                this,
                "PDF分割エラー",
                QString("分割後のPDFが見つかりません。\n%1")
                    .arg(splitPath));
            return false;
        }

        splitFiles.append(QFileInfo(splitPath));
    }

    QRegularExpression whiteSpace("\\s+");

    for (int i = 0; i < pageCount; ++i)
    {
        QString baseName =
            guidanceReportPdfEntries[i].studentName.trimmed() +
            guidanceReportPdfEntries[i].subject.trimmed() +
            ui->guidanceReportPdfDateEdit->date().toString("MMdd");
        baseName.remove(whiteSpace);
        baseName = sanitizeGuidanceReportPdfFileName(baseName);
        const QString destinationPath = uniqueGuidanceReportPdfPath(baseName);

        if (!QFile::rename(splitFiles[i].absoluteFilePath(), destinationPath))
        {
            QMessageBox::warning(
                this,
                "名前変更エラー",
                QString("PDFの名前を変更できませんでした。\n%1")
                    .arg(destinationPath));
            return false;
        }
    }

    return true;
}

// 指導報告書PDFの作業状態を初期化する
void MainWindow::resetGuidanceReportPdfWork()
{
    ui->guidanceReportPdfTeacherList->clearSelection();
    ui->guidanceReportPdfTeacherList->setCurrentItem(nullptr);
    ui->guidanceReportPdfStudentEdit->clear();
    ui->guidanceReportPdfSubjectEdit->clear();
    ui->guidanceReportPdfPageLabel->setText("PDFを選択してください");
    ui->guidanceReportPdfPreviousButton->setEnabled(false);
    ui->guidanceReportPdfNextButton->setEnabled(false);
    ui->guidanceReportPdfNextButton->setText("次へ進む");
    guidanceReportPdfEntries.clear();
    guidanceReportPdfCurrentPage = -1;
    guidanceReportPdfSourcePath.clear();

    if (guidanceReportPdfDocument != nullptr)
    {
        guidanceReportPdfDocument->close();
    }
}
