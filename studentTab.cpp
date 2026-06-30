#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QComboBox>
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
#include <QTextEdit>

#include <algorithm>

namespace
{
	QJsonObject studentToJson(const StudentData &student)
	{
		QJsonObject object;
		object["name"] = student.Name;
		object["grade"] = student.Grade;
		object["gender"] = student.gender;
		object["memo"] = student.memo;
		object["school"] = student.school;

		QJsonArray subjects;

		for (const QString &subject : student.subjects)
		{
			subjects.append(subject);
		}

		object["subjects"] = subjects;
		return object;
	}

	StudentData jsonToStudent(const QJsonObject &object)
	{
		StudentData student;
		student.Name = object.value("name").toString();
		student.Grade = object.value("grade").toInt();
		student.gender = object.value("gender").toInt();
		student.memo = object.value("memo").toString();
		student.school = object.value("school").toString();

		for (const QJsonValue &value : object.value("subjects").toArray())
		{
			student.subjects.append(value.toString());
		}

		return student;
	}

	int findGradeGroup(
		const QVector<GradeStudents> &allStudents,
		const QString &grade)
	{
		for (int i = 0; i < allStudents.size(); ++i)
		{
			if (allStudents[i].Grade == grade)
			{
				return i;
			}
		}

		return -1;
	}
}

bool MainWindow::saveStudentsToFile(const QVector<GradeStudents> &studentsToSave)
{
	QVector<GradeStudents> sortedStudents = studentsToSave;

	std::sort(
		sortedStudents.begin(),
		sortedStudents.end(),
		[this](const GradeStudents &a, const GradeStudents &b)
		{
			const int aIndex = grades.indexOf(a.Grade);
			const int bIndex = grades.indexOf(b.Grade);

			if (aIndex < 0 && bIndex < 0)
			{
				return a.Grade < b.Grade;
			}

			if (aIndex < 0)
			{
				return false;
			}

			if (bIndex < 0)
			{
				return true;
			}

			return aIndex < bIndex;
		});

	QJsonArray gradeArray;

	for (const GradeStudents &gradeStudents : sortedStudents)
	{
		QJsonObject gradeObject;
		gradeObject["grade"] = gradeStudents.Grade;

		QJsonArray studentsArray;

		for (const StudentData &student : gradeStudents.students)
		{
			studentsArray.append(studentToJson(student));
		}

		gradeObject["students"] = studentsArray;
		gradeArray.append(gradeObject);
	}

	QJsonObject root;
	root["version"] = 1;
	root["gradeStudents"] = gradeArray;

	QFile file(dataFilePath("students"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		return false;
	}

	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	return true;
}

void MainWindow::setupStudentTab()
{
	ui->studentGradeComboBox->clear();
	ui->studentGradeComboBox->addItem("");
	ui->studentGradeComboBox->addItems(grades);

	ui->studenGenderComboBox->clear();
	ui->studenGenderComboBox->addItem("");
	ui->studenGenderComboBox->addItems(genders);
	updateSchoolComboBox();

	ui->studentListView->setModel(
		new QStandardItemModel(ui->studentListView));

	connect(
		ui->studentApplyButton,
		&QPushButton::clicked,
		this,
		&MainWindow::saveStudent);

	connect(
		ui->studentDeleteButton,
		&QPushButton::clicked,
		this,
		&MainWindow::removeStudent);

	connect(
		ui->studentListView,
		&QListView::clicked,
		this,
		[this](const QModelIndex &index)
		{
			loadStudent(index.row());
		});

	connect(ui->actionAddschool, &QAction::triggered, this, &MainWindow::addSchoolList);
	connect(ui->actionDeleteSchool, &QAction::triggered, this, &MainWindow::deleteSchoolList);

	loadStudent();
	renderStudentList();
}

void MainWindow::renderStudentList()
{
	auto *model =
		qobject_cast<QStandardItemModel *>(ui->studentListView->model());

	if (model == nullptr)
	{
		return;
	}

	model->clear();

	for (int gradeIndex = 0; gradeIndex < allStudents.size(); ++gradeIndex)
	{
		const GradeStudents &gradeStudents = allStudents[gradeIndex];

		for (int studentIndex = 0;
			 studentIndex < gradeStudents.students.size();
			 ++studentIndex)
		{
			const StudentData &student =
				gradeStudents.students[studentIndex];

			auto *item = new QStandardItem(
				QString("%1 | %2").arg(gradeStudents.Grade, student.Name));

			item->setData(gradeIndex, Qt::UserRole);
			item->setData(studentIndex, Qt::UserRole + 1);
			model->appendRow(item);
		}
	}
}

void MainWindow::loadStudent(int index)
{
	auto *model =
		qobject_cast<QStandardItemModel *>(ui->studentListView->model());

	if (model == nullptr || index < 0 || index >= model->rowCount())
	{
		clearStudentEntry();
		return;
	}

	const QModelIndex modelIndex = model->index(index, 0);
	const int gradeIndex = modelIndex.data(Qt::UserRole).toInt();
	const int studentIndex = modelIndex.data(Qt::UserRole + 1).toInt();

	if (gradeIndex < 0 || gradeIndex >= allStudents.size() ||
		studentIndex < 0 ||
		studentIndex >= allStudents[gradeIndex].students.size())
	{
		clearStudentEntry();
		return;
	}

	const StudentData &student =
		allStudents[gradeIndex].students[studentIndex];

	ui->studentNameInput->setText(student.Name);
	ui->studentGradeComboBox->setCurrentText(allStudents[gradeIndex].Grade);
	ui->studenGenderComboBox->setCurrentIndex(student.gender);
	ui->studentSchoolComboBox->setCurrentText(student.school);
	ui->studentMemoTextEdit->setPlainText(student.memo);
}

void MainWindow::renderStudentEntry()
{
	loadStudent(ui->studentListView->currentIndex().row());
}

void MainWindow::clearStudentEntry()
{
	ui->studentListView->clearSelection();
	ui->studentNameInput->clear();
	ui->studentMemoTextEdit->clear();
}

void MainWindow::addStudent()
{
	clearStudentEntry();
	ui->studentNameInput->setFocus();
}

void MainWindow::removeStudent()
{
	const QModelIndex modelIndex = ui->studentListView->currentIndex();

	if (!modelIndex.isValid())
	{
		QMessageBox::information(
			this,
			"削除",
			"削除する生徒を一覧から選択してください。");
		return;
	}

	const int gradeIndex = modelIndex.data(Qt::UserRole).toInt();
	const int studentIndex = modelIndex.data(Qt::UserRole + 1).toInt();

	if (gradeIndex < 0 || gradeIndex >= allStudents.size() ||
		studentIndex < 0 ||
		studentIndex >= allStudents[gradeIndex].students.size())
	{
		return;
	}

	const QString name =
		allStudents[gradeIndex].students[studentIndex].Name;

	const auto answer = QMessageBox::question(
		this,
		"生徒を削除",
		QString("%1 を削除します。").arg(name),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);

	if (answer != QMessageBox::Yes)
	{
		return;
	}

	allStudents[gradeIndex].students.removeAt(studentIndex);

	if (allStudents[gradeIndex].students.isEmpty())
	{
		allStudents.removeAt(gradeIndex);
	}

	if (!saveStudentsToFile(allStudents))
	{
		QMessageBox::warning(
			this,
			"保存エラー",
			"生徒データを保存できませんでした。");
		return;
	}

	renderStudentList();
	clearStudentEntry();

	updateStudentComboBox(
		ui->student1ComboBox,
		ui->student1GradeComboBox->currentText());

	statusBar()->showMessage("生徒を削除しました", 2000);
}

void MainWindow::saveStudent()
{
	const QString name = ui->studentNameInput->text().trimmed();
	const QString grade = ui->studentGradeComboBox->currentText();

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

	StudentData student;
	student.Name = name;
	student.Grade = ui->studentGradeComboBox->currentIndex();
	student.gender = ui->studenGenderComboBox->currentIndex();
	student.memo = ui->studentMemoTextEdit->toPlainText();
	student.school = ui->studentSchoolComboBox->currentText().trimmed();

	bool isUpdate = false;
	const QModelIndex modelIndex = ui->studentListView->currentIndex();

	if (modelIndex.isValid())
	{
		const int oldGradeIndex = modelIndex.data(Qt::UserRole).toInt();
		const int oldStudentIndex =
			modelIndex.data(Qt::UserRole + 1).toInt();

		if (oldGradeIndex >= 0 && oldGradeIndex < allStudents.size() &&
			oldStudentIndex >= 0 &&
			oldStudentIndex < allStudents[oldGradeIndex].students.size())
		{
			student.subjects =
				allStudents[oldGradeIndex].students[oldStudentIndex].subjects;

			allStudents[oldGradeIndex].students.removeAt(oldStudentIndex);

			if (allStudents[oldGradeIndex].students.isEmpty())
			{
				allStudents.removeAt(oldGradeIndex);
			}

			isUpdate = true;
		}
	}

	int gradeIndex = findGradeGroup(allStudents, grade);

	if (gradeIndex < 0)
	{
		allStudents.append({grade, {}});
		gradeIndex = allStudents.size() - 1;
	}

	allStudents[gradeIndex].students.append(student);

	if (!saveStudentsToFile(allStudents))
	{
		QMessageBox::warning(
			this,
			"保存エラー",
			"生徒データを保存できませんでした。");
		return;
	}

	renderStudentList();
	clearStudentEntry();

	updateStudentComboBox(
		ui->student1ComboBox,
		ui->student1GradeComboBox->currentText());

	statusBar()->showMessage(
		isUpdate ? "生徒データを変更しました" : "生徒を追加しました",
		2000);
}

void MainWindow::loadStudent()
{
	allStudents.clear();

	QFile file(dataFilePath("students"));

	if (!file.exists())
	{
		return;
	}

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(
			this,
			"読み込みエラー",
			"生徒データを読み込めませんでした。");
		return;
	}

	QJsonParseError error;
	const QJsonDocument document =
		QJsonDocument::fromJson(file.readAll(), &error);

	if (error.error != QJsonParseError::NoError || !document.isObject())
	{
		QMessageBox::warning(
			this,
			"読み込みエラー",
			"生徒データの形式が正しくありません。");
		return;
	}

	for (const QJsonValue &gradeValue :
		 document.object().value("gradeStudents").toArray())
	{
		const QJsonObject gradeObject = gradeValue.toObject();
		GradeStudents gradeStudents;
		gradeStudents.Grade = gradeObject.value("grade").toString();

		if (gradeStudents.Grade.isEmpty())
		{
			continue;
		}

		for (const QJsonValue &studentValue :
			 gradeObject.value("students").toArray())
		{
			const StudentData student =
				jsonToStudent(studentValue.toObject());

			if (!student.Name.trimmed().isEmpty())
			{
				gradeStudents.students.append(student);
			}
		}

		if (!gradeStudents.students.isEmpty())
		{
			allStudents.append(gradeStudents);
		}
	}
}

void MainWindow::updateSchoolComboBox()
{
	const QString currentSchool = ui->studentSchoolComboBox->currentText();

	loadSchoolList();

	ui->studentSchoolComboBox->clear();
	ui->studentSchoolComboBox->addItem("");
	ui->studentSchoolComboBox->addItems(schools);
	ui->studentSchoolComboBox->setEditable(true);

	ui->studentSchoolComboBox->setCurrentText(currentSchool);
}

void MainWindow::addSchoolList()
{
	const QString school = ui->studentSchoolComboBox->currentText().trimmed();

	if (school.isEmpty())
	{
		QMessageBox::warning(this, "入力エラー", "追加する学校名を入力してください。");
		return;
	}

	if (schools.contains(school))
	{
		statusBar()->showMessage("すでに登録されている学校です", 2000);
		return;
	}

	schools.append(school);
	schools.sort();

	saveSchoolList();
	updateSchoolComboBox();

	ui->studentSchoolComboBox->setCurrentText(school);

	statusBar()->showMessage("学校を追加しました", 2000);
}

void MainWindow::deleteSchoolList()
{
	const QString school = ui->studentSchoolComboBox->currentText().trimmed();

	if (school.isEmpty())
	{
		QMessageBox::warning(this, "削除", "削除する学校を選択してください。");
		return;
	}

	if (!schools.contains(school))
	{
		statusBar()->showMessage("学校一覧に登録されていません", 2000);
		return;
	}

	const auto answer = QMessageBox::question(
		this,
		"学校を削除",
		QString("%1 を学校一覧から削除します。\n生徒に保存済みの学校名は変更されません。").arg(school),
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);

	if (answer != QMessageBox::Yes)
	{
		return;
	}

	schools.removeAll(school);

	saveSchoolList();
	updateSchoolComboBox();

	statusBar()->showMessage("学校を削除しました", 2000);
}

void MainWindow::saveSchoolList()
{
	QJsonArray schoolArray;

	for (const QString &school : schools)
	{
		const QString trimmedSchool = school.trimmed();

		if (!trimmedSchool.isEmpty())
		{
			schoolArray.append(trimmedSchool);
		}
	}

	QJsonObject root;
	root["version"] = 1;
	root["schools"] = schoolArray;

	QFile file(dataFilePath("school"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, "保存エラー", "学校一覧を保存できませんでした。");
		return;
	}

	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void MainWindow::loadSchoolList()
{
	schools.clear();

	QFile file(dataFilePath("school"));

	if (!file.exists())
	{
		return;
	}

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, "読み込みエラー", "学校一覧を読み込めませんでした。");
		return;
	}

	QJsonParseError error;
	const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

	if (error.error != QJsonParseError::NoError || !document.isObject())
	{
		QMessageBox::warning(this, "読み込みエラー", "学校一覧の形式が正しくありません。");
		return;
	}

	for (const QJsonValue &value : document.object().value("schools").toArray())
	{
		const QString school = value.toString().trimmed();

		if (!school.isEmpty() && !schools.contains(school))
		{
			schools.append(school);
		}
	}

	// schools.sort();
}
