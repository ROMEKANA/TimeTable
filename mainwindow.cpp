#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("塾時間割表");
    // resize(1500, 760);

    loadMasterData();

    setupActions();

    setupScheduleTab();
    setupStudentTab();
    setupTeacherTab();
    setupExportTab();

    ui->scheduleTable->setCurrentCell(0, 0);
}

MainWindow::~MainWindow()
{
    saveScheduleToFile();
    delete ui;
}

QString MainWindow::dataFilePath(QString data)
{
    QDir dir(QCoreApplication::applicationDirPath() + "/data");

    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    return dir.filePath(data + ".json");
}

void MainWindow::loadMasterData()
{
    QFile file(dataFilePath("master"));

    if (!file.exists())
    {
        QMessageBox::warning(
            this,
            "設定ファイルなし",
            "data/master.json が見つかりません。");
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "master.json を開けませんでした。");
        return;
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "master.json の形式が正しくありません。");
        return;
    }

    const QJsonObject root = document.object();

    auto readStringList = [&root](const QString &key)
    {
        QStringList result;

        for (const QJsonValue &value : root.value(key).toArray())
        {
            const QString text = value.toString().trimmed();

            if (!text.isEmpty())
            {
                result.append(text);
            }
        }

        return result;
    };

    days = readStringList("days");
    periods = readStringList("periods");
    grades = readStringList("grades");
    genders = readStringList("genders");
    subjects = readStringList("subjects");

    if (days.isEmpty() || periods.isEmpty())
    {
        QMessageBox::warning(
            this,
            "読み込みエラー",
            "days または periods が空です。");
    }

    auto readInt = [&root](const QString &key, int defaultValue) -> int
    {
        if (!root.contains(key) || !root[key].isDouble())
        {
            return defaultValue;
        }
        return root[key].toInt();
    };

    cellSectionSize = qMax(40, readInt("cellSectionSize", 115));
    MaxStudentPerTeacher = qMax(1, readInt("MaxStudentPerTeacher", 2));
    scheduleVerticalLineWidth =
        qMax(0, readInt("scheduleVerticalLineWidth", scheduleVerticalLineWidth));
    scheduleHorizontalLineWidth =
        qMax(0, readInt("scheduleHorizontalLineWidth", scheduleHorizontalLineWidth));
    scheduleVerticalSectionLineWidth =
        qMax(0, readInt("scheduleVerticalSectionLineWidth", scheduleVerticalSectionLineWidth));
    scheduleHorizontalSectionLineWidth =
        qMax(0, readInt("scheduleHorizontalSectionLineWidth", scheduleHorizontalSectionLineWidth));

    auto readColor = [&root](const QString &key, const QString &defaultValue) -> QString
    {
        const QString value = root.value(key).toString(defaultValue).trimmed();
        return QColor(value).isValid() ? value : defaultValue;
    };

    scheduleOddRowColor =
        readColor("scheduleOddRowColor", scheduleOddRowColor);
    scheduleVerticalLineColor =
        readColor("scheduleVerticalLineColor", scheduleVerticalLineColor);
    scheduleHorizontalLineColor =
        readColor("scheduleHorizontalLineColor", scheduleHorizontalLineColor);
    scheduleVerticalSectionLineColor =
        readColor("scheduleVerticalSectionLineColor", scheduleVerticalSectionLineColor);
    scheduleHorizontalSectionLineColor =
        readColor("scheduleHorizontalSectionLineColor", scheduleHorizontalSectionLineColor);

    auto readFloat = [&root](const QString &key, float defaultValue) -> float
    {
        if (!root.contains(key) || !root[key].isDouble())
        {
            return defaultValue;
        }
        return root[key].toDouble();
    };

    scrollSpeed = qMax(0.005, readFloat("scrollSpeed", 0.01));
}

void MainWindow::setupActions()
{
    connect(ui->actionScheduleLoad, &QAction::triggered, this, &MainWindow::loadScheduleButton);
    connect(ui->actionScheduleSave, &QAction::triggered, this, &MainWindow::saveScheduleToFile);
    connect(ui->actionSchedulePrint, &QAction::triggered, this, &MainWindow::showSchedulePrintPreview);

    connect(ui->actionCopyCell, &QAction::triggered, this, &MainWindow::copyCell);
    connect(ui->actionPasteCell, &QAction::triggered, this, &MainWindow::pasteCell);
    connect(ui->actionCutCell, &QAction::triggered, this, &MainWindow::cutCell);
    connect(ui->actionClearCell, &QAction::triggered, this, &MainWindow::clearCell);
    connect(ui->actionUndo, &QAction::triggered, this, &MainWindow::undoCellEdit);
    connect(ui->actionRedo, &QAction::triggered, this, &MainWindow::redoCellEdit);
}
