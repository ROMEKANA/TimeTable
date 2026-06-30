#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
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

void MainWindow::setupExportTab()
{
    connect(ui->printButton, &QPushButton::clicked, this, &MainWindow::showSchedulePrintPreview);
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
        const qreal leftX = rect.left() + verticalLineWidth / 2.0;
        const qreal rightX = rect.right() - verticalLineWidth / 2.0;

        painter->setPen(QPen(verticalLineColor, verticalLineWidth));
        painter->drawLine(
            QPointF(leftX, rect.top()),
            QPointF(leftX, rect.bottom()));
        painter->drawLine(
            QPointF(rightX, rect.top()),
            QPointF(rightX, rect.bottom()));
    }

    if (horizontalLineWidth > 0)
    {
        const qreal topY = rect.top() + horizontalLineWidth / 2.0;
        const qreal bottomY = rect.bottom() - horizontalLineWidth / 2.0;

        painter->setPen(QPen(horizontalLineColor, horizontalLineWidth));
        painter->drawLine(
            QPointF(rect.left(), topY),
            QPointF(rect.right(), topY));
        painter->drawLine(
            QPointF(rect.left(), bottomY),
            QPointF(rect.right(), bottomY));
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
