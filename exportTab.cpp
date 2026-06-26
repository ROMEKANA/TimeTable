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

	if (totalTeacherColumns == 0 || tableRowCount() == 0)
	{
		return;
	}

	const QRect pageRect =
		printer->pageLayout().paintRectPixels(printer->resolution());

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

	const qreal studentRowHeight =
		(tableHeight - headerHeight) / tableRowCount();

	QFont font = painter.font();
	font.setPointSize(8);
	painter.setFont(font);

	auto drawFittedText = [&painter](
							  const QRectF &rect,
							  const QString &text,
							  Qt::Alignment alignment,
							  bool bold = false)
	{
		QFont currentFont = painter.font();
		currentFont.setBold(bold);

		int pointSize = 12;

		while (pointSize >= 6)
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

		painter.setFont(currentFont);
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

		painter.drawRect(dayRect);

		const QDate date = scheduleMonday.addDays(dayIndex);
		const QString dayText = QString("%1\t%2")
									.arg(date.toString("M/d"))
									.arg(days.value(dayIndex));

		drawFittedText(
			dayRect.adjusted(3, 2, -3, -2),
			dayText,
			Qt::AlignCenter,
			true);

		for (int teacherIndex = 0;
			 teacherIndex < daySchedule.size();
			 ++teacherIndex)
		{
			const TeacherColumn &teacher = daySchedule[teacherIndex];

			const QRectF teacherRect(
				x + teacherColumnWidth * teacherIndex,
				area.top() + dayHeaderHeight,
				teacherColumnWidth,
				teacherHeaderHeight);

			painter.drawRect(teacherRect);

			const QString teacherName =
				teacher.teacherName.trimmed().isEmpty()
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

		painter.drawRect(timeRect);

		drawFittedText(
			timeRect.adjusted(3, 2, -3, -2),
			periods[periodIndex],
			Qt::AlignCenter);

		for (int studentIndex = 0;
			 studentIndex < MaxStudentPerTeacher;
			 ++studentIndex)
		{
			const qreal y = periodTop + studentRowHeight * studentIndex;
			x = area.left() + timeColumnWidth;

			for (const QVector<TeacherColumn> &daySchedule : schedule)
			{
				for (const TeacherColumn &teacher : daySchedule)
				{
					const QRectF cellRect(
						x,
						y,
						teacherColumnWidth,
						studentRowHeight);

					painter.drawRect(cellRect);

					QString text;

					if (periodIndex < teacher.lessons.size() &&
						studentIndex < teacher.lessons[periodIndex].size())
					{
						text = cellTextFromData(
							teacher.lessons[periodIndex][studentIndex]);
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
}
