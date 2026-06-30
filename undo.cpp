#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QStatusBar>

bool MainWindow::lessonDataEquals(const LessonData &a, const LessonData &b) const
{
    return a.studentName == b.studentName &&
           a.studentGrade == b.studentGrade &&
           a.subject == b.subject &&
           a.memo == b.memo;
}

bool MainWindow::setLessonAtCell(int row, int column, const LessonData &lesson)
{
    const int dayIndex = dayIndexFromColumn(column);
    const int teacherIndex = teacherIndexFromColumn(column);
    const int periodIndex = periodIndexFromTableRow(row);
    const int studentIndex = studentIndexFromTableRow(row);

    if (dayIndex < 0 || teacherIndex < 0 ||
        periodIndex < 0 || studentIndex < 0)
    {
        return false;
    }

    if (periodIndex >= schedule[dayIndex][teacherIndex].lessons.size() ||
        studentIndex >= schedule[dayIndex][teacherIndex].lessons[periodIndex].size())
    {
        return false;
    }

    schedule[dayIndex][teacherIndex].lessons[periodIndex][studentIndex] = lesson;

    return true;
}

void MainWindow::pushCellEdit(int row, int column, const LessonData &before, const LessonData &after)
{
    if (lessonDataEquals(before, after))
    {
        return;
    }

    undoStack.append({row, column, before, after});
    redoStack.clear();

    updateUndoRedoButtons();
}

void MainWindow::undoCellEdit()
{
    updateCell();

    if (undoStack.isEmpty())
    {
        return;
    }

    const CellEditCommand command = undoStack.takeLast();

    if (!setLessonAtCell(command.row, command.column, command.before))
    {
        clearCellEditHistory();
        return;
    }

    redoStack.append(command);

    selectedRow = command.row;
    selectedColumn = command.column;

    ui->scheduleTable->blockSignals(true);
    ui->scheduleTable->setCurrentCell(command.row, command.column);
    ui->scheduleTable->blockSignals(false);

    renderCell(command.row, command.column);

    isLoadingCell = true;
    renderEntry();
    isLoadingCell = false;

    updateUndoRedoButtons();

    statusBar()->showMessage("操作を元に戻しました", 2000);
}

void MainWindow::redoCellEdit()
{
    if (redoStack.isEmpty())
    {
        return;
    }

    const CellEditCommand command = redoStack.takeLast();

    if (!setLessonAtCell(command.row, command.column, command.after))
    {
        clearCellEditHistory();
        return;
    }

    undoStack.append(command);

    selectedRow = command.row;
    selectedColumn = command.column;

    ui->scheduleTable->blockSignals(true);
    ui->scheduleTable->setCurrentCell(command.row, command.column);
    ui->scheduleTable->blockSignals(false);

    renderCell(command.row, command.column);

    isLoadingCell = true;
    renderEntry();
    isLoadingCell = false;

    updateUndoRedoButtons();

    statusBar()->showMessage("操作をやり直しました", 2000);
}

void MainWindow::clearCellEditHistory()
{
    undoStack.clear();
    redoStack.clear();

    updateUndoRedoButtons();
}

void MainWindow::updateUndoRedoButtons()
{
    ui->undoButton->setEnabled(!undoStack.isEmpty());
    ui->redoButton->setEnabled(!redoStack.isEmpty());
}
