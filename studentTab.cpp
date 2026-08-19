#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QInputDialog>
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
	// カンマや読点で区切られた教材名を重複のない一覧にする
	QStringList splitMaterials(const QString &text)
	{
		QString normalized = text;
		normalized.replace("、", ",");
		normalized.replace("，", ",");

		QStringList materials;

		for (const QString &part : normalized.split(',', Qt::SkipEmptyParts))
		{
			const QString material = part.trimmed();

			if (!material.isEmpty() && !materials.contains(material))
			{
				materials.append(material);
			}
		}

		return materials;
	}

	// 教科マスターから教材未設定の生徒教科一覧を作る
	QVector<StudentSubjectData> defaultStudentSubjects(const QStringList &subjects)
	{
		QVector<StudentSubjectData> result;

		for (const QString &subject : subjects)
		{
			const QString subjectName = subject.trimmed();

			if (subjectName.isEmpty())
			{
				continue;
			}

			StudentSubjectData subjectData;
			subjectData.subjectName = subjectName;
			result.append(subjectData);
		}

		return result;
	}

	// 生徒の教科と教材を編集欄用の複数行テキストへ変換する
	QString studentSubjectsToText(
		const QVector<StudentSubjectData> &studentSubjects,
		const QStringList &defaultSubjects)
	{
		const QVector<StudentSubjectData> displaySubjects =
			studentSubjects.isEmpty()
				? defaultStudentSubjects(defaultSubjects)
				: studentSubjects;
		QStringList lines;

		for (const StudentSubjectData &subject : displaySubjects)
		{
			const QString subjectName = subject.subjectName.trimmed();

			if (subjectName.isEmpty())
			{
				continue;
			}

			QString line = subjectName + ":";

			if (!subject.materials.isEmpty())
			{
				line += " " + subject.materials.join(", ");
			}

			lines.append(line);
		}

		return lines.join('\n');
	}

	// 編集欄の複数行テキストから生徒の教科と教材を読み取る
	QVector<StudentSubjectData> studentSubjectsFromText(const QString &text)
	{
		QVector<StudentSubjectData> result;

		for (const QString &rawLine : text.split('\n'))
		{
			const QString line = rawLine.trimmed();

			if (line.isEmpty())
			{
				continue;
			}

			int separatorIndex = line.indexOf(':');
			const int fullWidthSeparatorIndex = line.indexOf(QChar(0xff1a));

			if (separatorIndex < 0 ||
				(fullWidthSeparatorIndex >= 0 &&
				 fullWidthSeparatorIndex < separatorIndex))
			{
				separatorIndex = fullWidthSeparatorIndex;
			}

			StudentSubjectData subject;

			if (separatorIndex >= 0)
			{
				subject.subjectName = line.left(separatorIndex).trimmed();
				subject.materials = splitMaterials(line.mid(separatorIndex + 1));
			}
			else
			{
				subject.subjectName = line;
			}

			if (!subject.subjectName.isEmpty())
			{
				result.append(subject);
			}
		}

		return result;
	}

	// 生徒データを保存用JSONへ変換する
	QJsonObject studentToJson(const StudentData &student)
	{
		QJsonObject object;
		object["name"] = student.Name;
		object["grade"] = student.Grade;
		object["gender"] = student.gender;
		object["memo"] = student.memo;
		object["school"] = student.school;

		QJsonArray subjects;

		for (const StudentSubjectData &subject : student.subjects)
		{
			QJsonObject subjectObject;
			subjectObject["subjectName"] = subject.subjectName;

			QJsonArray materials;

			for (const QString &material : subject.materials)
			{
				const QString materialName = material.trimmed();

				if (!materialName.isEmpty())
				{
					materials.append(materialName);
				}
			}

			subjectObject["materials"] = materials;
			subjects.append(subjectObject);
		}

		object["subjects"] = subjects;
		return object;
	}

	// JSONから新旧形式に対応した生徒データを読み取る
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
			StudentSubjectData subject;

			if (value.isString())
			{
				subject.subjectName = value.toString();
			}
			else if (value.isObject())
			{
				const QJsonObject subjectObject = value.toObject();
				subject.subjectName =
					subjectObject.value("subjectName").toString(
						subjectObject.value("name").toString());

				for (const QJsonValue &materialValue :
					 subjectObject.value("materials").toArray())
				{
					const QString material =
						materialValue.toString().trimmed();

					if (!material.isEmpty())
					{
						subject.materials.append(material);
					}
				}
			}

			if (!subject.subjectName.trimmed().isEmpty())
			{
				student.subjects.append(subject);
			}
		}

		return student;
	}

	// 指定学年の生徒グループがある位置を返す
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

	// 2つの教科・教材一覧の内容と順序が同じか確認する
	bool studentSubjectsEqual(
		const QVector<StudentSubjectData> &a,
		const QVector<StudentSubjectData> &b)
	{
		if (a.size() != b.size())
		{
			return false;
		}

		for (int i = 0; i < a.size(); ++i)
		{
			if (a[i].subjectName != b[i].subjectName ||
				a[i].materials != b[i].materials)
			{
				return false;
			}
		}

		return true;
	}

	// 2つの生徒データの内容が同じか確認する
	bool studentDataEqual(const StudentData &a, const StudentData &b)
	{
		return a.Name == b.Name &&
			a.Grade == b.Grade &&
			a.gender == b.gender &&
			a.memo == b.memo &&
			a.school == b.school &&
			studentSubjectsEqual(a.subjects, b.subjects);
	}
}

// 生徒を学年順・名前順に並べてファイルへ保存する
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

	for (GradeStudents &gradeStudents : sortedStudents)
	{
		std::stable_sort(
			gradeStudents.students.begin(),
			gradeStudents.students.end(),
			[](const StudentData &a, const StudentData &b)
			{
				return QString::localeAwareCompare(a.Name, b.Name) < 0;
			});
	}

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
	root["version"] = 2;
	root["gradeStudents"] = gradeArray;

	QFile file(dataFilePath("students"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		return false;
	}

	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
	return true;
}

// 生徒タブの入力欄・一覧・操作を初期化する
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
	ui->studentListView->installEventFilter(this);
	ui->studentListView->viewport()->installEventFilter(this);
	ui->studentMemoTextEdit->installEventFilter(this);
	ui->studentMemoTextEdit->viewport()->installEventFilter(this);

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
		ui->studentScheduleFromListButton,
		&QPushButton::clicked,
		this,
		&MainWindow::copySelectedStudentScheduleToClipboard);

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
	clearStudentEntry();
}

// 学年と生徒名の一覧を現在のデータで描画する
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

// 一覧で指定された生徒を編集欄へ読み込む
void MainWindow::loadStudent(int index)
{
	auto *model =
		qobject_cast<QStandardItemModel *>(ui->studentListView->model());

	if (model == nullptr || index < 0 || index >= model->rowCount())
	{
		clearStudentEntry();
		return;
	}

	const QModelIndex requestedModelIndex = model->index(index, 0);
	const int requestedGradeIndex =
		requestedModelIndex.data(Qt::UserRole).toInt();
	const int requestedStudentIndex =
		requestedModelIndex.data(Qt::UserRole + 1).toInt();

	if (requestedGradeIndex < 0 ||
		requestedGradeIndex >= allStudents.size() ||
		requestedStudentIndex < 0 ||
		requestedStudentIndex >=
			allStudents[requestedGradeIndex].students.size())
	{
		clearStudentEntry();
		return;
	}

	if (loadedStudentGradeIndex == requestedGradeIndex &&
		loadedStudentIndex == requestedStudentIndex)
	{
		return;
	}

	const QString requestedGrade = allStudents[requestedGradeIndex].Grade;
	const QString requestedStudentName =
		allStudents[requestedGradeIndex].students[requestedStudentIndex].Name;

	if (!confirmStudentEditorChanges())
	{
		bool restoredSelection = false;

		for (int row = 0; row < model->rowCount(); ++row)
		{
			const QModelIndex rowIndex = model->index(row, 0);

			if (rowIndex.data(Qt::UserRole).toInt() == loadedStudentGradeIndex &&
				rowIndex.data(Qt::UserRole + 1).toInt() == loadedStudentIndex)
			{
				ui->studentListView->setCurrentIndex(rowIndex);
				restoredSelection = true;
				break;
			}
		}

		if (!restoredSelection)
		{
			ui->studentListView->clearSelection();
			ui->studentListView->setCurrentIndex(QModelIndex());
		}

		return;
	}

	model = qobject_cast<QStandardItemModel *>(ui->studentListView->model());

	if (model == nullptr)
	{
		clearStudentEntry();
		return;
	}

	index = -1;

	for (int row = 0; row < model->rowCount(); ++row)
	{
		const QModelIndex rowIndex = model->index(row, 0);
		const int rowGradeIndex = rowIndex.data(Qt::UserRole).toInt();
		const int rowStudentIndex = rowIndex.data(Qt::UserRole + 1).toInt();

		if (rowGradeIndex < 0 || rowGradeIndex >= allStudents.size() ||
			rowStudentIndex < 0 ||
			rowStudentIndex >= allStudents[rowGradeIndex].students.size())
		{
			continue;
		}

		const GradeStudents &gradeStudents = allStudents[rowGradeIndex];
		const StudentData &student = gradeStudents.students[rowStudentIndex];

		if (gradeStudents.Grade == requestedGrade &&
			student.Name == requestedStudentName)
		{
			index = row;
			ui->studentListView->setCurrentIndex(rowIndex);
			break;
		}
	}

	if (index < 0)
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
	ui->studentSubjectsTextEdit->setPlainText(
		studentSubjectsToText(student.subjects, subjects));
	ui->studentMemoTextEdit->setPlainText(student.memo);
	loadedStudentGradeIndex = gradeIndex;
	loadedStudentIndex = studentIndex;
	loadedStudentGrade = allStudents[gradeIndex].Grade;
	loadedStudent = studentFromEditor();
}

// 選択中の生徒を編集欄へ再表示する
void MainWindow::renderStudentEntry()
{
	loadStudent(ui->studentListView->currentIndex().row());
}

// 生徒の選択と編集欄を新規入力状態へ戻す
void MainWindow::clearStudentEntry()
{
	loadedStudentGradeIndex = -1;
	loadedStudentIndex = -1;
	ui->studentListView->clearSelection();
	ui->studentListView->setCurrentIndex(QModelIndex());
	ui->studentNameInput->clear();
	ui->studentGradeComboBox->setCurrentIndex(0);
	ui->studenGenderComboBox->setCurrentIndex(0);
	ui->studentSchoolComboBox->setCurrentText("");
	ui->studentSubjectsTextEdit->setPlainText(
		studentSubjectsToText(QVector<StudentSubjectData>(), subjects));
	ui->studentMemoTextEdit->clear();
	loadedStudentGrade = ui->studentGradeComboBox->currentText();
	loadedStudent = studentFromEditor();
}

// 選択中の生徒を確認後に削除する
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

	const bool wasLoadingCell = isLoadingCell;
	isLoadingCell = true;
	updateStudentComboBox(
		ui->student1ComboBox,
		ui->student1GradeComboBox->currentText(),
		true);
	updateSubjectComboBoxForStudent(
		ui->student1SubjectComboBox,
		ui->student1GradeComboBox->currentText(),
		ui->student1ComboBox->currentText(),
		true);
	isLoadingCell = wasLoadingCell;

	statusBar()->showMessage("生徒を削除しました", 2000);
}

// 生徒編集欄の入力内容をデータとして取得する
StudentData MainWindow::studentFromEditor() const
{
	StudentData student;
	student.Name = ui->studentNameInput->text().trimmed();
	student.Grade = ui->studentGradeComboBox->currentIndex();
	student.gender = ui->studenGenderComboBox->currentIndex();
	student.memo = ui->studentMemoTextEdit->toPlainText();
	student.school = ui->studentSchoolComboBox->currentText().trimmed();
	student.subjects =
		studentSubjectsFromText(ui->studentSubjectsTextEdit->toPlainText());
	return student;
}

// 生徒編集欄に未反映の変更があるか確認する
bool MainWindow::studentEditorHasChanges() const
{
	return loadedStudentGrade != ui->studentGradeComboBox->currentText() ||
		!studentDataEqual(loadedStudent, studentFromEditor());
}

// 生徒編集欄の変更を反映するか確認する
bool MainWindow::confirmStudentEditorChanges()
{
	if (!studentEditorHasChanges())
	{
		return true;
	}

	const auto answer = QMessageBox::question(
		this,
		"生徒データの変更",
		"選択中の生徒データが変更されています。\n変更を反映しますか？",
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
		QMessageBox::Yes);

	if (answer == QMessageBox::Cancel)
	{
		return false;
	}

	if (answer == QMessageBox::Yes)
	{
		return saveStudentFromEditor();
	}

	return true;
}

// 生徒編集欄の内容を更新または新規追加して保存する
bool MainWindow::saveStudentFromEditor()
{
	const QString name = ui->studentNameInput->text().trimmed();
	const QString grade = ui->studentGradeComboBox->currentText();

	if (name.isEmpty())
	{
		QMessageBox::warning(this, "入力エラー", "生徒名を入力してください。");
		return false;
	}

	if (grade.isEmpty())
	{
		QMessageBox::warning(this, "入力エラー", "学年を選択してください。");
		return false;
	}

	StudentData student = studentFromEditor();
	QVector<GradeStudents> updatedStudents = allStudents;

	const bool isUpdate =
		loadedStudentGradeIndex >= 0 &&
		loadedStudentGradeIndex < updatedStudents.size() &&
		loadedStudentIndex >= 0 &&
		loadedStudentIndex <
			updatedStudents[loadedStudentGradeIndex].students.size();

	if (isUpdate && updatedStudents[loadedStudentGradeIndex].Grade == grade)
	{
		updatedStudents[loadedStudentGradeIndex].students[loadedStudentIndex] =
			student;
	}
	else
	{
		if (isUpdate)
		{
			updatedStudents[loadedStudentGradeIndex].students.removeAt(
				loadedStudentIndex);

			if (updatedStudents[loadedStudentGradeIndex].students.isEmpty())
			{
				updatedStudents.removeAt(loadedStudentGradeIndex);
			}
		}

		int gradeIndex = findGradeGroup(updatedStudents, grade);

		if (gradeIndex < 0)
		{
			updatedStudents.append({grade, {}});
			gradeIndex = updatedStudents.size() - 1;
		}

		updatedStudents[gradeIndex].students.append(student);
	}

	if (!saveStudentsToFile(updatedStudents))
	{
		QMessageBox::warning(
			this,
			"保存エラー",
			"生徒データを保存できませんでした。");
		return false;
	}

	loadStudent();
	renderStudentList();
	clearStudentEntry();

	const bool wasLoadingCell = isLoadingCell;
	isLoadingCell = true;
	updateStudentComboBox(
		ui->student1ComboBox,
		ui->student1GradeComboBox->currentText(),
		true);
	updateSubjectComboBoxForStudent(
		ui->student1SubjectComboBox,
		ui->student1GradeComboBox->currentText(),
		ui->student1ComboBox->currentText(),
		true);
	isLoadingCell = wasLoadingCell;

	statusBar()->showMessage(
		isUpdate ? "生徒データを変更しました" : "生徒を追加しました",
		2000);
	return true;
}

// 生徒編集欄の内容を保存する
void MainWindow::saveStudent()
{
	saveStudentFromEditor();
}

// 生徒一覧をファイルから読み込む
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

// 指定した生徒に登録された教科名を返す
QStringList MainWindow::subjectNamesForStudent(
	const QString &grade,
	const QString &studentName) const
{
	StudentData student;

	if (!findStudentData(grade, studentName, &student))
	{
		return subjects;
	}

	QStringList result;

	for (const StudentSubjectData &subject : student.subjects)
	{
		const QString subjectName = subject.subjectName.trimmed();

		if (!subjectName.isEmpty() && !result.contains(subjectName))
		{
			result.append(subjectName);
		}
	}

	return result.isEmpty() ? subjects : result;
}

// 指定した生徒と教科に登録された教材名を返す
QStringList MainWindow::materialNamesForStudentSubject(
	const QString &grade,
	const QString &studentName,
	const QString &subjectName) const
{
	StudentData student;

	if (!findStudentData(grade, studentName, &student))
	{
		return {};
	}

	for (const StudentSubjectData &subject : student.subjects)
	{
		if (subject.subjectName == subjectName)
		{
			return subject.materials;
		}
	}

	return {};
}

// 学校一覧を再読込して選択肢を更新する
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

// 入力された学校名を学校一覧へ追加する
void MainWindow::addSchoolList()
{
	const QString currentSchool = ui->studentSchoolComboBox->currentText().trimmed();
	const QString school = QInputDialog::getText(
		this,
		"学校の追加",
		"追加する学校名",
		QLineEdit::Normal,
		currentSchool)
							   .trimmed();

	if (school.isEmpty())
	{
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

// 選択された学校名を確認後に学校一覧から削除する
void MainWindow::deleteSchoolList()
{
	loadSchoolList();

	if (schools.isEmpty())
	{
        QMessageBox::warning(this, "エラー", "削除できる学校がありません");
		return;
	}

	const QString currentSchool = ui->studentSchoolComboBox->currentText().trimmed();
	const int currentIndex = qMax(0, schools.indexOf(currentSchool));

	bool ok = false;
	const QString school = QInputDialog::getItem(
		this,
		"学校の削除",
		"削除する学校",
		schools,
		currentIndex,
		false,
		&ok)
							   .trimmed();

	if (!ok || school.isEmpty())
	{
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

// 学校一覧をファイルへ保存する
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

// 学校一覧をファイルから読み込む
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
