#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::setupExportTab()
{
    connect(
        ui->printButton,
        &QPushButton::clicked,
        this,
        &MainWindow::showSchedulePrintPreview);

    connect(
        ui->actionSchedulePrint,
        &QAction::triggered,
        this,
        &MainWindow::showSchedulePrintPreview);
}

void MainWindow::showSchedulePrintPreview()
{
    if (schedule.isEmpty() || periods.isEmpty())
    {
        QMessageBox::warning(
            this,
            "印刷できません",
            "時間割データがありません。");
        return;
    }

    /*
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this,
            "印刷プレビュー",
            "現在の時間割表の印刷プレビューを開きますか？",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes
        );

        if(answer != QMessageBox::Yes){
            return;
        }
    */

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

void MainWindow::renderScheduleForPrint(QPrinter *printer)
{
    QPainter painter(printer);

    if (!painter.isActive())
    {
        return;
    }

    int totalTeacherColumns = 0;

    for (const QVector<TeacherColumn> &daySchedule : schedule)
    {
        totalTeacherColumns += daySchedule.size();
    }

    if (totalTeacherColumns == 0)
    {
        return;
    }

    const QRect pageRect = printer->pageLayout().paintRectPixels(printer->resolution());

    const int margin = 40;
    const QRectF area = pageRect.adjusted(margin, margin, -margin, -margin);

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

    const qreal tableWidth = area.width();
    const qreal tableHeight = area.height();

    const qreal teacherColumnWidth =
        (tableWidth - timeColumnWidth) / totalTeacherColumns;

    const qreal rowHeight =
        (tableHeight - headerHeight) / periods.size();

    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    auto drawFittedText = [&painter](
                              const QRectF &rect,
                              const QString &text,
                              Qt::Alignment alignment,
                              bool bold = false)
    {
        QFont font = painter.font();
        font.setBold(bold);

        int pointSize = 12;

        while (pointSize >= 10)
        {
            font.setPointSize(pointSize);

            QFontMetrics metrics(font);

            const QRect textRect = metrics.boundingRect(
                rect.toRect(),
                alignment | Qt::TextWordWrap,
                text);

            if (textRect.width() <= rect.width() && textRect.height() <= rect.height())
            {
                break;
            }

            pointSize--;
        }

        painter.setFont(font);

        painter.drawText(
            rect,
            alignment | Qt::TextWordWrap,
            text);
    };

    painter.drawRect(
        QRectF(
            area.left(),
            area.top(),
            timeColumnWidth,
            headerHeight));

    painter.drawText(
        QRectF(
            area.left(),
            area.top(),
            timeColumnWidth,
            headerHeight),
        Qt::AlignCenter,
        "時間");

    QFont headerFont = painter.font();
    headerFont.setPointSize(10);
    headerFont.setBold(true);
    painter.setFont(headerFont);

    qreal x = area.left() + timeColumnWidth;

    for (int dayIndex = 0; dayIndex < schedule.size(); dayIndex++)
    {
        const QVector<TeacherColumn> &daySchedule = schedule[dayIndex];

        if (daySchedule.isEmpty())
        {
            continue;
        }

        const qreal dayWidth = teacherColumnWidth * daySchedule.size();

        QRectF dayRect(
            x,
            area.top(),
            dayWidth,
            dayHeaderHeight);

        painter.drawRect(dayRect);

        drawFittedText(
            dayRect.adjusted(3, 2, -3, -2),
            days.value(dayIndex),
            Qt::AlignCenter,
            true);

        for (int teacherIndex = 0; teacherIndex < daySchedule.size(); teacherIndex++)
        {
            const TeacherColumn &teacher = daySchedule[teacherIndex];

            QRectF teacherRect(
                x + teacherColumnWidth * teacherIndex,
                area.top() + dayHeaderHeight,
                teacherColumnWidth,
                teacherHeaderHeight);

            painter.drawRect(teacherRect);

            const QString teacherName =
                teacher.teacherName.isEmpty()
                    ? "講師未設定"
                    : teacher.teacherName;

            drawFittedText(
                teacherRect.adjusted(3, 2, -3, -2),
                teacherName,
                Qt::AlignCenter,
                true);
        }

        x += dayWidth;
    }

    QFont bodyFont = painter.font();
    bodyFont.setPointSize(8);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);

    for (int row = 0; row < periods.size(); row++)
    {
        const qreal y = area.top() + headerHeight + rowHeight * row;

        QRectF timeRect(
            area.left(),
            y,
            timeColumnWidth,
            rowHeight);

        painter.drawRect(timeRect);

        drawFittedText(
            timeRect.adjusted(3, 2, -3, -2),
            periods[row],
            Qt::AlignCenter);

        x = area.left() + timeColumnWidth;

        for (int dayIndex = 0; dayIndex < schedule.size(); dayIndex++)
        {
            const QVector<TeacherColumn> &daySchedule = schedule[dayIndex];

            for (int teacherIndex = 0; teacherIndex < daySchedule.size(); teacherIndex++)
            {
                QRectF cellRect(
                    x,
                    y,
                    teacherColumnWidth,
                    rowHeight);

                painter.drawRect(cellRect);

                QString text;

                if (row < daySchedule[teacherIndex].lessons.size())
                {
                    text = cellTextFromData(
                        daySchedule[teacherIndex].lessons[row]);
                }

                drawFittedText(
                    cellRect.adjusted(5, 4, -5, -4),
                    text,
                    Qt::AlignLeft | Qt::AlignVCenter);

                x += teacherColumnWidth;
            }
        }
    }
}