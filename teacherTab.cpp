#include "mainwindow.h"
#include "ui_mainwindow.h"

namespace
{
    QJsonObject teacherToJson(QString &teacher)
    {
        QJsonObject object;
        object["name"] = teacher;

        return object;
    }

    QString jsonToTeacher(const QJsonObject &object)
    {
        return object.value("name").toString();
    }
}

bool MainWindow::saveTeachersToFile()
{
    QJsonArray Array;

    for (auto teacher : teachers)
    {
        QJsonObject Object = teacherToJson(teacher);
        Array.append(Object);
    }

    QJsonObject root;
    root["version"] = 1;
    root["Teachers"] = Array;

    QFile file(dataFilePath("teachers"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void MainWindow::setupTeacherTab()
{
    ui->teacherListView->setModel(new QStandardItemModel(ui->teacherListView));

    connect(ui->teacherApplyButton, &QPushButton::clicked, this, &MainWindow::saveTeacher);
    connect(ui->teacherDeleteButton, &QPushButton::clicked, this, &MainWindow::removeTeacher);
    connect(ui->teacherListView, &QListView::clicked, this, [this](const QModelIndex &index)
            { loadTeacher(index.row()); });

    loadTeacher();
    renderTeacherList();
}

void MainWindow::renderTeacherList()
{
    auto *model = qobject_cast<QStandardItemModel *>(ui->teacherListView->model());
    if (model == nullptr)
    {
        return;
    }

    model->clear();

    for (int index = 0; index < teachers.size(); ++index)
    {
        const QString &teacher = teachers[index];
        auto *item = new QStandardItem(teacher);
        item->setData(index, Qt::UserRole);
        item->setData(index, Qt::UserRole + 1);
        model->appendRow(item);
    }
}

void MainWindow::loadTeacher(int index)
{
    auto *model = qobject_cast<QStandardItemModel *>(ui->teacherListView->model());
    if (model == nullptr || index < 0 || index >= model->rowCount())
    {
        clearTeacherEntry();
        return;
    }

    const QModelIndex modelIndex = model->index(index, 0);
    const int index = modelIndex.data(Qt::UserRole).toInt();
    const int teacherIndex = modelIndex.data(Qt::UserRole + 1).toInt();

    if (index < 0 || index >= teachers.size() ||
        teacherIndex < 0 || teacherIndex >= teachers.size())
    {
        clearTeacherEntry();
        return;
    }

    const QString &teacher = teachers[teacherIndex];
    ui->teacherNameInput->setText(teacher);
    //ui->studenGenderComboBox->setCurrentIndex();
}

void MainWindow::renderTeacherEntry()
{
    loadTeacher(ui->teacherListView->currentIndex().row());
}

void MainWindow::clearTeacherEntry()
{
    ui->teacherListView->clearSelection();
    ui->teacherNameInput->clear();
    //ui->studenGenderComboBox->setCurrentIndex(0);
}

void MainWindow::addTeacher()
{
    clearTeacherEntry();
    ui->teacherNameInput->setFocus();
}

void MainWindow::removeTeacher()
{
    const QModelIndex modelIndex = ui->teacherListView->currentIndex();
    if (!modelIndex.isValid())
    {
        QMessageBox::information(this, "削除", "削除する生徒を一覧から選択してください。");
        return;
    }

    const int index = modelIndex.data(Qt::UserRole).toInt();
    const int teacherIndex = modelIndex.data(Qt::UserRole + 1).toInt();

    if (index < 0 || index >= teachers.size() ||
        teacherIndex < 0 || teacherIndex >= teachers.size())
    {
        return;
    }

    const QString name = teachers[teacherIndex];
    const auto answer = QMessageBox::question(
        this,
        "講師を削除",
        QString("%1 を削除します。").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);


    if (answer != QMessageBox::Yes)
    {
        return;
    }

    teachers.removeAt(teacherIndex);
    if (teachers.isEmpty())
    {
        teachers.removeAt(index);
    }

    if (!saveTeachersToFile())
    {
        QMessageBox::warning(this, "保存エラー", "生徒データを保存できませんでした。");
    }

    renderTeacherList();
    clearTeacherEntry();
    statusBar()->showMessage("生徒を削除しました", 2000);
}

void MainWindow::saveTeacher()
{
    const QString name = ui->teacherNameInput->text().trimmed();

    if (name.isEmpty())
    {
        QMessageBox::warning(this, "入力エラー", "生徒名を入力してください。");
        return;
    }

    if (grade.isEmpty())
    {
        QMessageBox::warning(this, "入力エラー", "学年を選択してください。");
        return;
    }

    QString teacher;
    teacher = name;
    //teacher.gender = ui->studenGenderComboBox->currentIndex();
    //teacher.memo = ui->teacherMemoTextEdit->toPlainText();

    bool isUpdate = false;
    const QModelIndex modelIndex = ui->teacherListView->currentIndex();

    if (modelIndex.isValid())
    {
        const int oldGradeIndex = modelIndex.data(Qt::UserRole).toInt();
        const int oldTeacherIndex = modelIndex.data(Qt::UserRole + 1).toInt();

        if (oldGradeIndex >= 0 && oldGradeIndex < teachers.size() &&
            oldTeacherIndex >= 0 && oldTeacherIndex < teachers[oldGradeIndex].teachers.size())
        {

            if (teachers[oldGradeIndex].isEmpty())
            {
                teachers.removeAt(oldGradeIndex);
            }

            isUpdate = true;
        }
    }

    teachers.append(teacher);

    if (!saveTeachersToFile())
    {
        QMessageBox::warning(this, "保存エラー", "生徒データを保存できませんでした。");
        return;
    }

    renderTeacherList();
    clearTeacherEntry();
    statusBar()->showMessage(isUpdate ? "生徒データを変更しました" : "生徒を追加しました", 2000);
}

void MainWindow::loadTeacher()
{
    teachers.clear();

    QFile file(dataFilePath("teachers"));
    if (!file.exists())
    {
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "読み込みエラー", "生徒データを読み込めませんでした。");
        return;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        QMessageBox::warning(this, "読み込みエラー", "生徒データの形式が正しくありません。");
        return;
    }

    for (const QJsonValue &gradeValue : document.object().value("gradeTeachers").toArray())
    {
        const QJsonObject 
    }
}
