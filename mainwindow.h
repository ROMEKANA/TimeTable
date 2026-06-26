#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringList>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>

QT_BEGIN_NAMESPACE
namespace Ui
{
	class MainWindow;
}
QT_END_NAMESPACE

struct LessonData
{
	QString studentName;
	QString studentGrade;
	QString subject;
	QString memo;
};

struct TeacherColumn
{
	QString teacherName;
	QVector<QVector<LessonData>> lessons;
};

struct StudentData
{
	QString Name;
	int Grade;
	int gender;
	QString memo;
	QStringList subjects;
	QString school;
};

struct GradeStudents
{
	QString Grade;
	QVector<StudentData> students;
};

struct TeacherData
{
	QString name;
	QString memo;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT

protected:
	bool eventFilter(QObject *object, QEvent *event) override;

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

private:
	Ui::MainWindow *ui;

	QStringList days;
	QStringList periods;

	QStringList grades;
	QStringList genders;
	QStringList subjects;

	QVector<GradeStudents> allStudents;
	QVector<TeacherData> teachers;

	QVector<QVector<TeacherColumn>> schedule;

	int MaxStudentPerTeacher = 2;
	float scrollSpeed = 0.01f;

	QString dataFilePath(QString data);
	void loadMasterData();

	int selectedRow = -1;
	int selectedColumn = -1;
	int cellDefaultSectionSize = 115;

	QDate scheduleMonday;

	void setupTable();
	void scheduleTabConnects();

	void initializeTeacherLessons(TeacherColumn &teacher);
	void initializeTable();
	void renderTable();

	int tableRowCount() const;
	int periodIndexFromTableRow(int tableRow) const;
	int studentIndexFromTableRow(int tableRow) const;
	int tableRowOf(int periodIndex, int studentIndex) const;

	int firstColumnOfDay(int dayIndex) const;
	int columnCountOfDay(int dayIndex) const;
	int dayIndexFromColumn(int column) const;
	int teacherIndexFromColumn(int column) const;

	void addTeacherColumn();
	void removeTeacherColumn();
	void renameTeacherColumn();

	void loadCell(int row, int column);
	void updateCell();
	QString cellTextFromData(const LessonData &lesson) const;
	void renderCell(int row, int column);
	void clearCell();
	void renderEntry();

	void copyCell();
	void pasteCell();
    void cutCell();

	bool lessonDataIsEmpty(const LessonData &lesson) const;

	void updateStudentComboBox(QComboBox *comboBox, const QString &grade);
	void updateTeacherComboBox(QComboBox *comboBox);

	QString lessonToJson(const LessonData &lesson) const;
	QString lessonToJson(int row, int column) const;
	LessonData jsonToLesson(const QString &json) const;

	QString scheduleToJson() const;
	bool jsonToSchedule(const QString &json);

	void saveScheduleToFile();
	void loadScheduleButton();
	void loadLatestSchedule();
	bool loadScheduleFromFile(const QDate &monday);

	void switchScheduleWeek(const QDate &date);
	void showLastWeek();
	void showThisWeek();
	void showNextWeek();

	QDate mondayOf(const QDate &date) const;
	QString schedulesDirPath() const;
	QString scheduleFilePath(const QDate &monday);

	void setupStudentTab();
	void renderStudentList();
	void loadStudent(int index);
	void renderStudentEntry();
	void clearStudentEntry();
	void addStudent();
	void removeStudent();
	void saveStudent();
	void loadStudent();
	bool saveStudentsToFile(const QVector<GradeStudents> &allStudents);

	void setupTeacherTab();
	void renderTeacherList();
	void loadTeacher(int index);
	void renderTeacherEntry();
	void clearTeacherEntry();
	void addTeacher();
	void removeTeacher();
	void saveTeacher();
	void loadTeacher();
	bool saveTeachersToFile();

	void setupExportTab();
	void showSchedulePrintPreview();
	void renderScheduleForPrint(QPrinter *printer);
};

#endif // MAINWINDOW_H
