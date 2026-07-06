#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLineEdit>
#include <QListView>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QSpinBox>
#include <QTextEdit>

namespace
{
	QJsonObject teacherToJson(const TeacherData &teacher)
	{
		QJsonObject object;
		object["name"] = teacher.name;
		object["memo"] = teacher.memo;
		object["oneOnTwoRate"] = teacher.oneOnTwoRate;
		object["oneOnOneRate"] = teacher.oneOnOneRate;
		object["transportPay"] = teacher.transportPay;
		object["highSchoolAllowance"] = teacher.highSchoolAllowance;
		return object;
	}

	TeacherData jsonToTeacher(
		const QJsonObject &object,
		int defaultOneOnTwoRate,
		int defaultOneOnOneRate,
		int defaultTransportPay,
		int defaultHighSchoolAllowance)
	{
		TeacherData teacher;
		teacher.name = object.value("name").toString();
		teacher.memo = object.value("memo").toString();
		teacher.oneOnTwoRate =
			qMax(0, object.value("oneOnTwoRate").toInt(defaultOneOnTwoRate));
		teacher.oneOnOneRate =
			qMax(0, object.value("oneOnOneRate").toInt(defaultOneOnOneRate));
		teacher.transportPay =
			qMax(0, object.value("transportPay").toInt(defaultTransportPay));
		teacher.highSchoolAllowance =
			qMax(0, object.value("highSchoolAllowance").toInt(defaultHighSchoolAllowance));
		return teacher;
	}
}

bool MainWindow::saveTeachersToFile()
{
	QJsonArray teachersArray;

	for (const TeacherData &teacher : teachers)
	{
		teachersArray.append(teacherToJson(teacher));
	}

	QJsonObject root;
	root["version"] = 2;
	root["teachers"] = teachersArray;

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
	ui->teacherListView->installEventFilter(this);
	ui->teacherListView->viewport()->installEventFilter(this);
	ui->teacherMemoTextEdit->installEventFilter(this);
	ui->teacherMemoTextEdit->viewport()->installEventFilter(this);

	connect(ui->teacherApplyButton, &QPushButton::clicked, this, &MainWindow::saveTeacher);
	connect(ui->teacherDeleteButton, &QPushButton::clicked, this, &MainWindow::removeTeacher);
	connect(
		ui->teacherScheduleFromListButton,
		&QPushButton::clicked,
		this,
		&MainWindow::copySelectedTeacherScheduleToClipboard);
	connect(ui->teacherListView, &QListView::clicked, this, [this](const QModelIndex &index)
			{ loadTeacher(index.row()); });

	loadTeacher();
	renderTeacherList();
	clearTeacherEntry();
}

void MainWindow::renderTeacherList()
{
	auto *model = qobject_cast<QStandardItemModel *>(ui->teacherListView->model());
	if (model == nullptr)
	{
		return;
	}

	model->clear();

	for (int i = 0; i < teachers.size(); ++i)
	{
		auto *item = new QStandardItem(teachers[i].name);
		item->setData(i, Qt::UserRole);
		model->appendRow(item);
	}
}

void MainWindow::loadTeacher(int row)
{
	auto *model = qobject_cast<QStandardItemModel *>(ui->teacherListView->model());
	if (model == nullptr || row < 0 || row >= model->rowCount())
	{
		clearTeacherEntry();
		return;
	}

	const int teacherIndex = model->index(row, 0).data(Qt::UserRole).toInt();
	if (teacherIndex < 0 || teacherIndex >= teachers.size())
	{
		clearTeacherEntry();
		return;
	}

	const TeacherData &teacher = teachers[teacherIndex];
	ui->teacherNameInput->setText(teacher.name);
	ui->teacherOneOnTwoRateSpinBox->setValue(teacher.oneOnTwoRate);
	ui->teacherOneOnOneRateSpinBox->setValue(teacher.oneOnOneRate);
	ui->teacherTransportPaySpinBox->setValue(teacher.transportPay);
	ui->teacherHighSchoolAllowanceSpinBox->setValue(teacher.highSchoolAllowance);
	ui->teacherMemoTextEdit->setPlainText(teacher.memo);
}

void MainWindow::renderTeacherEntry()
{
	loadTeacher(ui->teacherListView->currentIndex().row());
}

void MainWindow::clearTeacherEntry()
{
	ui->teacherListView->clearSelection();
	ui->teacherNameInput->clear();
	ui->teacherOneOnTwoRateSpinBox->setValue(defaultSalaryOneOnTwoRate);
	ui->teacherOneOnOneRateSpinBox->setValue(defaultSalaryOneOnOneRate);
	ui->teacherTransportPaySpinBox->setValue(defaultSalaryTransportPay);
	ui->teacherHighSchoolAllowanceSpinBox->setValue(defaultSalaryHighSchoolAllowance);
	ui->teacherMemoTextEdit->clear();
}

void MainWindow::removeTeacher()
{
	const QModelIndex modelIndex = ui->teacherListView->currentIndex();
	if (!modelIndex.isValid())
	{
		QMessageBox::information(this, "削除", "削除する講師を一覧から選択してください。");
		return;
	}

	const int teacherIndex = modelIndex.data(Qt::UserRole).toInt();
	if (teacherIndex < 0 || teacherIndex >= teachers.size())
	{
		return;
	}

	const QString name = teachers[teacherIndex].name;
	const auto answer = QMessageBox::question(
		this,
		"講師を削除",
		QString("%1 を削除します。\nこの操作は取り消せません。").arg(name),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);

	if (answer != QMessageBox::Yes)
	{
		return;
	}

	teachers.removeAt(teacherIndex);

	if (!saveTeachersToFile())
	{
		QMessageBox::warning(this, "保存エラー", "講師データを保存できませんでした。");
		return;
	}

	renderTeacherList();
	clearTeacherEntry();
	statusBar()->showMessage("講師を削除しました", 2000);

	updateTeacherComboBox(ui->teacherComboBox);
}

void MainWindow::saveTeacher()
{
	const QString name = ui->teacherNameInput->text().trimmed();

	if (name.isEmpty())
	{
		QMessageBox::warning(this, "入力エラー", "講師名を入力してください。");
		return;
	}

	TeacherData teacher;
	teacher.name = name;
	teacher.memo = ui->teacherMemoTextEdit->toPlainText();
	teacher.oneOnTwoRate = ui->teacherOneOnTwoRateSpinBox->value();
	teacher.oneOnOneRate = ui->teacherOneOnOneRateSpinBox->value();
	teacher.transportPay = ui->teacherTransportPaySpinBox->value();
	teacher.highSchoolAllowance = ui->teacherHighSchoolAllowanceSpinBox->value();

	bool isUpdate = false;
	const QModelIndex modelIndex = ui->teacherListView->currentIndex();

	if (modelIndex.isValid())
	{
		const int teacherIndex = modelIndex.data(Qt::UserRole).toInt();

		if (teacherIndex >= 0 && teacherIndex < teachers.size())
		{
			teachers[teacherIndex] = teacher;
			isUpdate = true;
		}
	}

	if (!isUpdate)
	{
		teachers.append(teacher);
	}

	if (!saveTeachersToFile())
	{
		QMessageBox::warning(this, "保存エラー", "講師データを保存できませんでした。");
		return;
	}

	renderTeacherList();
	clearTeacherEntry();
	updateTeacherComboBox(ui->teacherComboBox);
	statusBar()->showMessage(isUpdate ? "講師データを変更しました" : "講師を追加しました", 2000);
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
		QMessageBox::warning(this, "読み込みエラー", "講師データを読み込めませんでした。");
		return;
	}

	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

	if (error.error != QJsonParseError::NoError || !document.isObject())
	{
		QMessageBox::warning(this, "読み込みエラー", "講師データの形式が正しくありません。");
		return;
	}

	for (const QJsonValue &value : document.object().value("teachers").toArray())
	{
		const TeacherData teacher =
			jsonToTeacher(
				value.toObject(),
				defaultSalaryOneOnTwoRate,
				defaultSalaryOneOnOneRate,
				defaultSalaryTransportPay,
				defaultSalaryHighSchoolAllowance);

		if (!teacher.name.trimmed().isEmpty())
		{
			teachers.append(teacher);
		}
	}
}
