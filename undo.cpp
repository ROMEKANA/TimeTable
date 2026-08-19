#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QMessageBox>
#include <QStatusBar>

// 2つの授業データの内容が同じか確認する
bool MainWindow::lessonDataEquals(const LessonData &a, const LessonData &b) const
{
    return a.studentName == b.studentName &&
           a.studentGrade == b.studentGrade &&
           a.subject == b.subject &&
           a.memo == b.memo &&
           a.maxStudents == b.maxStudents;
}

// 指定セルに対応する授業データを更新する
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

// セル編集をUndo履歴へ追加し、Redo履歴を破棄する
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

// 直前のセル編集を元に戻す
void MainWindow::undoCellEdit()
{
    if (!ensureScheduleEditable("元に戻す操作"))
    {
        return;
    }

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

    const int periodIndex = periodIndexFromTableRow(command.row);
    const int firstRow = tableRowOf(periodIndex, 0);

    for (int studentRow = 0; studentRow < MaxStudentPerTeacher; ++studentRow)
    {
        renderCell(firstRow + studentRow, command.column);
    }

    isLoadingCell = true;
    renderEntry();
    isLoadingCell = false;

    updateUndoRedoButtons();

    statusBar()->showMessage("操作を元に戻しました", 2000);
}

// 元に戻したセル編集をやり直す
void MainWindow::redoCellEdit()
{
    if (!ensureScheduleEditable("やり直す操作"))
    {
        return;
    }

    updateCell();

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

    const int periodIndex = periodIndexFromTableRow(command.row);
    const int firstRow = tableRowOf(periodIndex, 0);

    for (int studentRow = 0; studentRow < MaxStudentPerTeacher; ++studentRow)
    {
        renderCell(firstRow + studentRow, command.column);
    }

    isLoadingCell = true;
    renderEntry();
    isLoadingCell = false;

    updateUndoRedoButtons();

    statusBar()->showMessage("操作をやり直しました", 2000);
}

// セル編集のUndo・Redo履歴を消去する
void MainWindow::clearCellEditHistory()
{
    undoStack.clear();
    redoStack.clear();

    updateUndoRedoButtons();
}

// 履歴が消える操作を続けるか確認する
bool MainWindow::confirmClearCellEditHistory(const QString &operationName)
{
    if (undoStack.isEmpty() && redoStack.isEmpty())
    {
        return true;
    }

    const auto answer = QMessageBox::question(
        this,
        operationName,
        operationName + "を行うと、元に戻す履歴が消えます。\n続けますか？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    return answer == QMessageBox::Yes;
}

// Undo・Redoボタンの有効状態を履歴に合わせる
void MainWindow::updateUndoRedoButtons()
{
    ui->undoButton->setEnabled(!undoStack.isEmpty());
    ui->redoButton->setEnabled(!redoStack.isEmpty());
}

// 現在の時間割が保存済みファイルと一致するか確認する
bool MainWindow::scheduleMatchesSavedFile()
{
    if (!scheduleMonday.isValid())
    {
        return false;
    }

    QFile file(scheduleFilePath(scheduleMonday));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    const QString savedJson = QString::fromUtf8(file.readAll());

    return savedJson == scheduleToJson();
}

// 未保存の時間割を保存するか確認する
bool MainWindow::confirmSaveScheduleChanges(const QString &operationName)
{
    updateCell();

    if (scheduleMatchesSavedFile())
    {
        return true;
    }

    const auto answer = QMessageBox::question(
        this,
        operationName,
        "保存している時間割と現在の内容が違います。\n保存しますか？",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes);

    if (answer == QMessageBox::Cancel)
    {
        return false;
    }

    if (answer == QMessageBox::Yes)
    {
        return saveScheduleToFile();
    }

    return true;
}
