#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAction>
#include <QByteArray>
#include <QCloseEvent>
#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QStatusBar>
#include <QVBoxLayout>

namespace
{
    enum class MasterFieldType
    {
        Int,
        Double,
        Color
    };

    struct MasterField
    {
        QString key;
        QString label;
        MasterFieldType type;
        int defaultInt = 0;
        int minInt = 0;
        int maxInt = 999999;
        double defaultDouble = 0.0;
        double minDouble = 0.0;
        double maxDouble = 999999.0;
        QString defaultText;
    };

    QJsonArray stringListToJsonArray(const QStringList &values)
    {
        QJsonArray array;

        for (const QString &value : values)
        {
            const QString text = value.trimmed();

            if (!text.isEmpty())
            {
                array.append(text);
            }
        }

        return array;
    }

    QStringList stringListFromText(const QString &text)
    {
        QStringList result;

        for (const QString &line : text.split('\n'))
        {
            const QString value = line.trimmed();

            if (!value.isEmpty() && !result.contains(value))
            {
                result.append(value);
            }
        }

        return result;
    }

    QStringList stringListFromJsonArray(
        const QJsonObject &root,
        const QString &key)
    {
        QStringList result;

        for (const QJsonValue &value : root.value(key).toArray())
        {
            const QString text = value.toString().trimmed();

            if (!text.isEmpty() && !result.contains(text))
            {
                result.append(text);
            }
        }

        return result;
    }

    QString colorText(
        const QJsonObject &root,
        const QString &key,
        const QString &defaultValue)
    {
        const QString text = root.value(key).toString(defaultValue).trimmed();
        return QColor(text).isValid() ? text : defaultValue;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("塾時間割表");
    // resize(1500, 760);

    loadMasterData();
    loadApplicationState();

    setupActions();

    setupScheduleTab();
    setupStudentTab();
    setupTeacherTab();
    setupExportTab();

    ui->scheduleTable->setCurrentCell(0, 0);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    updateCell();

    if (scheduleMatchesSavedFile())
    {
        saveApplicationState();
        event->accept();
        return;
    }

    const auto answer = QMessageBox::question(
        this,
        "時間割の保存",
        "保存している時間割と現在の内容が違います。\n保存しますか？",
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes);

    if (answer == QMessageBox::Cancel)
    {
        event->ignore();
        return;
    }

    if (answer == QMessageBox::Yes && !saveScheduleToFile())
    {
        event->ignore();
        return;
    }

    saveApplicationState();
    event->accept();
}

void MainWindow::loadApplicationState()
{
    QFile file(dataFilePath("appState"));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return;
    }

    const QJsonObject root = document.object();
    const QDate savedMonday =
        QDate::fromString(root.value("scheduleMonday").toString(), "yyyy-MM-dd");

    if (savedMonday.isValid())
    {
        startupScheduleMonday = mondayOf(savedMonday);
    }

    const QByteArray geometry =
        QByteArray::fromBase64(root.value("windowGeometry").toString().toLatin1());

    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }

    const int tabIndex = root.value("mainTabIndex").toInt(-1);

    if (tabIndex >= 0 && tabIndex < ui->mainTabWidget->count())
    {
        ui->mainTabWidget->setCurrentIndex(tabIndex);
    }
}

bool MainWindow::saveApplicationState()
{
    QJsonObject root;
    root["scheduleMonday"] = scheduleMonday.toString("yyyy-MM-dd");
    root["mainTabIndex"] = ui->mainTabWidget->currentIndex();
    root["windowGeometry"] =
        QString::fromLatin1(saveGeometry().toBase64());

    QFile file(dataFilePath("appState"));

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
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
    QJsonObject root = loadMasterJson();
    normalizeMasterJson(&root);
    saveMasterJson(root);

    days = stringListFromJsonArray(root, "days");
    periods = stringListFromJsonArray(root, "periods");
    grades = stringListFromJsonArray(root, "grades");
    genders = stringListFromJsonArray(root, "genders");
    subjects = stringListFromJsonArray(root, "subjects");

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
    scheduleDisplayFontPointSize =
        qBound(6, readInt("scheduleDisplayFontPointSize", scheduleDisplayFontPointSize), 48);
    scheduleDisplayHeaderFontPointSize =
        qBound(6, readInt("scheduleDisplayHeaderFontPointSize", scheduleDisplayHeaderFontPointSize), 48);
    scheduleDisplayCellHeight =
        qBound(0, readInt("scheduleDisplayCellHeight", scheduleDisplayCellHeight), 500);
    scheduleDisplayHeaderHeight =
        qBound(18, readInt("scheduleDisplayHeaderHeight", scheduleDisplayHeaderHeight), 300);
    scheduleDisplayTimeHeaderWidth =
        qBound(24, readInt("scheduleDisplayTimeHeaderWidth", scheduleDisplayTimeHeaderWidth), 300);
    scheduleVerticalLineWidth =
        qMax(0, readInt("scheduleVerticalLineWidth", scheduleVerticalLineWidth));
    scheduleHorizontalLineWidth =
        qMax(0, readInt("scheduleHorizontalLineWidth", scheduleHorizontalLineWidth));
    scheduleVerticalSectionLineWidth =
        qMax(0, readInt("scheduleVerticalSectionLineWidth", scheduleVerticalSectionLineWidth));
    scheduleHorizontalSectionLineWidth =
        qMax(0, readInt("scheduleHorizontalSectionLineWidth", scheduleHorizontalSectionLineWidth));
    schedulePrintDarknessPercent =
        qMax(1, readInt("schedulePrintDarknessPercent", schedulePrintDarknessPercent));
    schedulePrintLineWidthPercent =
        qMax(1, readInt("schedulePrintLineWidthPercent", schedulePrintLineWidthPercent));
    schedulePrintSizePercent =
        qBound(50, readInt("schedulePrintSizePercent", schedulePrintSizePercent), 100);
    schedulePrintFontPointSize =
        qBound(5, readInt("schedulePrintFontPointSize", schedulePrintFontPointSize), 48);
    schedulePrintHeaderFontPointSize =
        qBound(5, readInt("schedulePrintHeaderFontPointSize", schedulePrintHeaderFontPointSize), 48);
    schedulePrintTimeColumnPadding =
        qBound(0, readInt("schedulePrintTimeColumnPadding", schedulePrintTimeColumnPadding), 500);
    schedulePrintDayHeaderHeight =
        qBound(20, readInt("schedulePrintDayHeaderHeight", schedulePrintDayHeaderHeight), 500);
    schedulePrintTeacherHeaderHeight =
        qBound(20, readInt("schedulePrintTeacherHeaderHeight", schedulePrintTeacherHeaderHeight), 500);
    schedulePrintAutoShrinkText =
        qBound(0, readInt("schedulePrintAutoShrinkText", schedulePrintAutoShrinkText), 1);
    studentSelectionVisibleRowCount =
        qBound(1, readInt("studentSelectionVisibleRowCount", studentSelectionVisibleRowCount), 30);
    defaultSalaryOneOnTwoRate =
        qMax(0, readInt("salaryOneOnTwoRate", defaultSalaryOneOnTwoRate));
    defaultSalaryOneOnOneRate =
        qMax(0, readInt("salaryOneOnOneRate", defaultSalaryOneOnOneRate));
    defaultSalaryHighSchoolAllowance =
        qMax(0, readInt("salaryHighSchoolAllowance", defaultSalaryHighSchoolAllowance));
    defaultSalaryTransportPay =
        qMax(0, readInt("salaryTransportPay", defaultSalaryTransportPay));

    auto readColor = [&root](const QString &key, const QString &defaultValue) -> QString
    {
        const QString value = root.value(key).toString(defaultValue).trimmed();
        return QColor(value).isValid() ? value : defaultValue;
    };

    scheduleOddRowColor =
        readColor("scheduleOddRowColor", scheduleOddRowColor);
    scheduleTextColor =
        readColor("scheduleTextColor", scheduleTextColor);
    scheduleOddRowTextColor =
        readColor("scheduleOddRowTextColor", scheduleOddRowTextColor);
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

QJsonObject MainWindow::loadMasterJson()
{
    QFile file(dataFilePath("master"));

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError || !document.isObject())
    {
        return {};
    }

    return document.object();
}

QStringList MainWindow::masterListDefaultValues(const QString &key) const
{
    if (key == "days")
    {
        return days;
    }

    if (key == "periods")
    {
        return periods;
    }

    if (key == "grades")
    {
        return grades;
    }

    if (key == "genders")
    {
        return genders;
    }

    if (key == "subjects")
    {
        return subjects;
    }

    return {};
}

void MainWindow::normalizeMasterJson(QJsonObject *root) const
{
    if (root == nullptr)
    {
        return;
    }

    auto normalizeList = [this, root](const QString &key)
    {
        QStringList values = stringListFromJsonArray(*root, key);

        if (values.isEmpty())
        {
            values = masterListDefaultValues(key);
        }

        (*root)[key] = stringListToJsonArray(values);
    };

    auto normalizeInt = [root](
                            const QString &key,
                            int defaultValue,
                            int minValue,
                            int maxValue)
    {
        (*root)[key] = qBound(
            minValue,
            root->value(key).toInt(defaultValue),
            maxValue);
    };

    auto normalizeDouble = [root](
                               const QString &key,
                               double defaultValue,
                               double minValue,
                               double maxValue)
    {
        (*root)[key] = qBound(
            minValue,
            root->value(key).toDouble(defaultValue),
            maxValue);
    };

    auto normalizeColor = [root](
                              const QString &key,
                              const QString &defaultValue)
    {
        const QString text =
            root->value(key).toString(defaultValue).trimmed();
        (*root)[key] = QColor(text).isValid() ? text : defaultValue;
    };

    normalizeList("days");
    normalizeList("periods");
    normalizeList("grades");
    normalizeList("genders");
    normalizeList("subjects");

    normalizeInt("cellSectionSize", cellSectionSize, 40, 2000);
    normalizeInt("MaxStudentPerTeacher", MaxStudentPerTeacher, 1, 20);
    normalizeDouble("scrollSpeed", scrollSpeed, 0.005, 10.0);
    normalizeInt("scheduleDisplayFontPointSize", scheduleDisplayFontPointSize, 6, 48);
    normalizeInt("scheduleDisplayHeaderFontPointSize", scheduleDisplayHeaderFontPointSize, 6, 48);
    normalizeInt("scheduleDisplayCellHeight", scheduleDisplayCellHeight, 0, 500);
    normalizeInt("scheduleDisplayHeaderHeight", scheduleDisplayHeaderHeight, 18, 300);
    normalizeInt("scheduleDisplayTimeHeaderWidth", scheduleDisplayTimeHeaderWidth, 24, 300);

    normalizeColor("scheduleOddRowColor", scheduleOddRowColor);
    normalizeColor("scheduleTextColor", scheduleTextColor);
    normalizeColor("scheduleOddRowTextColor", scheduleOddRowTextColor);
    normalizeColor("scheduleVerticalLineColor", scheduleVerticalLineColor);
    normalizeInt("scheduleVerticalLineWidth", scheduleVerticalLineWidth, 0, 20);
    normalizeColor("scheduleHorizontalLineColor", scheduleHorizontalLineColor);
    normalizeInt("scheduleHorizontalLineWidth", scheduleHorizontalLineWidth, 0, 20);
    normalizeColor("scheduleVerticalSectionLineColor", scheduleVerticalSectionLineColor);
    normalizeInt("scheduleVerticalSectionLineWidth", scheduleVerticalSectionLineWidth, 0, 30);
    normalizeColor("scheduleHorizontalSectionLineColor", scheduleHorizontalSectionLineColor);
    normalizeInt("scheduleHorizontalSectionLineWidth", scheduleHorizontalSectionLineWidth, 0, 30);

    normalizeInt("schedulePrintDarknessPercent", schedulePrintDarknessPercent, 1, 300);
    normalizeInt("schedulePrintLineWidthPercent", schedulePrintLineWidthPercent, 1, 300);
    normalizeInt("schedulePrintSizePercent", schedulePrintSizePercent, 50, 100);
    normalizeInt("schedulePrintFontPointSize", schedulePrintFontPointSize, 5, 48);
    normalizeInt("schedulePrintHeaderFontPointSize", schedulePrintHeaderFontPointSize, 5, 48);
    normalizeInt("schedulePrintTimeColumnPadding", schedulePrintTimeColumnPadding, 0, 500);
    normalizeInt("schedulePrintDayHeaderHeight", schedulePrintDayHeaderHeight, 20, 500);
    normalizeInt("schedulePrintTeacherHeaderHeight", schedulePrintTeacherHeaderHeight, 20, 500);
    normalizeInt("schedulePrintAutoShrinkText", schedulePrintAutoShrinkText, 0, 1);
    normalizeInt("studentSelectionVisibleRowCount", studentSelectionVisibleRowCount, 1, 30);

    normalizeInt("salaryOneOnTwoRate", defaultSalaryOneOnTwoRate, 0, 999999);
    normalizeInt("salaryOneOnOneRate", defaultSalaryOneOnOneRate, 0, 999999);
    normalizeInt("salaryHighSchoolAllowance", defaultSalaryHighSchoolAllowance, 0, 999999);
    normalizeInt("salaryTransportPay", defaultSalaryTransportPay, 0, 999999);
    root->remove("salaryBusinessPay");
}

bool MainWindow::saveMasterJson(const QJsonObject &root)
{
    QJsonObject normalizedRoot = root;
    normalizeMasterJson(&normalizedRoot);

    QFile file(dataFilePath("master"));

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "保存エラー", "master.json を保存できませんでした。");
        return false;
    }

    file.write(QJsonDocument(normalizedRoot).toJson(QJsonDocument::Indented));
    return true;
}

void MainWindow::refreshAfterMasterDataChanged()
{
    loadMasterData();

    schedule.resize(days.size());

    for (QVector<TeacherColumn> &daySchedule : schedule)
    {
        if (daySchedule.isEmpty())
        {
            TeacherColumn emptyColumn;
            daySchedule.append(emptyColumn);
        }

        for (TeacherColumn &teacher : daySchedule)
        {
            teacher.lessons.resize(periods.size());

            for (QVector<LessonData> &periodLessons : teacher.lessons)
            {
                periodLessons.resize(MaxStudentPerTeacher);
            }
        }
    }

    ui->studentGradeComboBox->clear();
    ui->studentGradeComboBox->addItem("");
    ui->studentGradeComboBox->addItems(grades);

    ui->studenGenderComboBox->clear();
    ui->studenGenderComboBox->addItem("");
    ui->studenGenderComboBox->addItems(genders);

    clearStudentEntry();

    if (!ui->teacherListView->currentIndex().isValid())
    {
        clearTeacherEntry();
    }

    renderTable();
    clearCellEditHistory();
}

void MainWindow::editMasterListValues(const QString &key, const QString &label)
{
    QJsonObject root = loadMasterJson();
    normalizeMasterJson(&root);

    const QStringList currentValues = stringListFromJsonArray(root, key);

    QDialog dialog(this);
    dialog.setWindowTitle(label + "の編集");
    dialog.resize(420, 420);

    QVBoxLayout layout(&dialog);
    auto *editor = new QPlainTextEdit(&dialog);
    editor->setPlainText(currentValues.join('\n'));
    layout.addWidget(editor);

    QDialogButtonBox buttonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        &dialog);
    layout.addWidget(&buttonBox);

    connect(
        &buttonBox,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);
    connect(
        &buttonBox,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    QStringList values = stringListFromText(editor->toPlainText());

    if (values.isEmpty())
    {
        return;
    }

    if (!confirmClearCellEditHistory(label + "の編集"))
    {
        return;
    }

    root[key] = stringListToJsonArray(values);

    if (!confirmClearCellEditHistory("マスターデータの管理"))
    {
        return;
    }

    if (!saveMasterJson(root))
    {
        return;
    }

    refreshAfterMasterDataChanged();
    statusBar()->showMessage(label + "を保存しました", 2000);
}

void MainWindow::showMasterDataDialog()
{
    QJsonObject root = loadMasterJson();
    normalizeMasterJson(&root);

    const QVector<MasterField> fields = {
        {"cellSectionSize", "【画面表示】時間割セル横幅", MasterFieldType::Int, cellSectionSize, 40, 2000},
        {"scheduleDisplayCellHeight", "【画面表示】時間割セル縦幅（0で自動）", MasterFieldType::Int, scheduleDisplayCellHeight, 0, 500},
        {"scheduleDisplayFontPointSize", "【画面表示】時間割セル文字サイズ", MasterFieldType::Int, scheduleDisplayFontPointSize, 6, 48},
        {"scheduleDisplayHeaderFontPointSize", "【画面表示】時間割ヘッダー文字サイズ", MasterFieldType::Int, scheduleDisplayHeaderFontPointSize, 6, 48},
        {"scheduleDisplayHeaderHeight", "【画面表示】横ヘッダー高さ", MasterFieldType::Int, scheduleDisplayHeaderHeight, 18, 300},
        {"scheduleDisplayTimeHeaderWidth", "【画面表示】縦ヘッダー幅", MasterFieldType::Int, scheduleDisplayTimeHeaderWidth, 24, 300},
        {"MaxStudentPerTeacher", "【画面表示】1コマ最大生徒数", MasterFieldType::Int, MaxStudentPerTeacher, 1, 20},
        {"scrollSpeed", "【画面表示】横スクロール速度", MasterFieldType::Double, 0, 0, 0, scrollSpeed, 0.005, 10.0},

        {"scheduleOddRowColor", "【画面・印刷共通】奇数行の網掛け色", MasterFieldType::Color, 0, 0, 0, 0.0, 0.0, 0.0, scheduleOddRowColor},
        {"scheduleTextColor", "【画面・印刷共通】通常行の文字色", MasterFieldType::Color, 0, 0, 0, 0.0, 0.0, 0.0, scheduleTextColor},
        {"scheduleOddRowTextColor", "【画面・印刷共通】奇数行の文字色", MasterFieldType::Color, 0, 0, 0, 0.0, 0.0, 0.0, scheduleOddRowTextColor},
        {"scheduleVerticalLineColor", "【画面・印刷共通】縦線の色", MasterFieldType::Color, 0, 0, 0, 0.0, 0.0, 0.0, scheduleVerticalLineColor},
        {"scheduleVerticalLineWidth", "【画面・印刷共通】縦線の太さ", MasterFieldType::Int, scheduleVerticalLineWidth, 0, 20},
        {"scheduleHorizontalLineColor", "【画面・印刷共通】横線の色", MasterFieldType::Color, 0, 0, 0, 0.0, 0.0, 0.0, scheduleHorizontalLineColor},
        {"scheduleHorizontalLineWidth", "【画面・印刷共通】横線の太さ", MasterFieldType::Int, scheduleHorizontalLineWidth, 0, 20},
        {"scheduleVerticalSectionLineColor", "【画面・印刷共通】曜日区切り縦線の色", MasterFieldType::Color, 0, 0, 0, 0.0, 0.0, 0.0, scheduleVerticalSectionLineColor},
        {"scheduleVerticalSectionLineWidth", "【画面・印刷共通】曜日区切り縦線の太さ", MasterFieldType::Int, scheduleVerticalSectionLineWidth, 0, 30},
        {"scheduleHorizontalSectionLineColor", "【画面・印刷共通】時限区切り横線の色", MasterFieldType::Color, 0, 0, 0, 0.0, 0.0, 0.0, scheduleHorizontalSectionLineColor},
        {"scheduleHorizontalSectionLineWidth", "【画面・印刷共通】時限区切り横線の太さ", MasterFieldType::Int, scheduleHorizontalSectionLineWidth, 0, 30},

        {"schedulePrintDarknessPercent", "【印刷】濃さ(%)", MasterFieldType::Int, schedulePrintDarknessPercent, 1, 300},
        {"schedulePrintLineWidthPercent", "【印刷】線幅(%)", MasterFieldType::Int, schedulePrintLineWidthPercent, 1, 300},
        {"schedulePrintSizePercent", "【印刷】時間割全体サイズ(%)", MasterFieldType::Int, schedulePrintSizePercent, 50, 100},
        {"schedulePrintFontPointSize", "【印刷】時間割セル文字サイズ", MasterFieldType::Int, schedulePrintFontPointSize, 5, 48},
        {"schedulePrintHeaderFontPointSize", "【印刷】時間割ヘッダー文字サイズ", MasterFieldType::Int, schedulePrintHeaderFontPointSize, 5, 48},
        {"schedulePrintTimeColumnPadding", "【印刷】時間列の追加幅", MasterFieldType::Int, schedulePrintTimeColumnPadding, 0, 500},
        {"schedulePrintDayHeaderHeight", "【印刷】曜日ヘッダー高さ", MasterFieldType::Int, schedulePrintDayHeaderHeight, 20, 500},
        {"schedulePrintTeacherHeaderHeight", "【印刷】講師ヘッダー高さ", MasterFieldType::Int, schedulePrintTeacherHeaderHeight, 20, 500},
        {"schedulePrintAutoShrinkText", "【印刷】文字が入らない時に自動縮小（0=オフ、1=オン）", MasterFieldType::Int, schedulePrintAutoShrinkText, 0, 1},
        {"studentSelectionVisibleRowCount", "【選択ダイアログ】生徒名・教科リスト表示行数", MasterFieldType::Int, studentSelectionVisibleRowCount, 1, 30},

        {"salaryOneOnTwoRate", "【給与】1:2コマ給の既定値", MasterFieldType::Int, defaultSalaryOneOnTwoRate, 0, 999999},
        {"salaryOneOnOneRate", "【給与】1:1コマ給の既定値", MasterFieldType::Int, defaultSalaryOneOnOneRate, 0, 999999},
        {"salaryHighSchoolAllowance", "【給与】高校生手当の既定値", MasterFieldType::Int, defaultSalaryHighSchoolAllowance, 0, 999999},
        {"salaryTransportPay", "【給与】交通費の既定値", MasterFieldType::Int, defaultSalaryTransportPay, 0, 999999}};

    QDialog dialog(this);
    dialog.setWindowTitle("マスターデータの管理");
    dialog.resize(720, 720);

    QVBoxLayout dialogLayout(&dialog);
    QScrollArea scrollArea(&dialog);
    QWidget formWidget(&scrollArea);
    QFormLayout formLayout(&formWidget);

    struct FieldEditor
    {
        MasterField field;
        QWidget *widget;
    };

    QVector<FieldEditor> editors;

    for (const MasterField &field : fields)
    {
        auto *editor = new QLineEdit(&formWidget);

        if (field.type == MasterFieldType::Int)
        {
            const int value = qBound(
                field.minInt,
                root.value(field.key).toInt(field.defaultInt),
                field.maxInt);
            editor->setText(QString::number(value));
        }
        else if (field.type == MasterFieldType::Double)
        {
            const double value = qBound(
                field.minDouble,
                root.value(field.key).toDouble(field.defaultDouble),
                field.maxDouble);
            editor->setText(QString::number(value));
        }
        else
        {
            editor->setText(colorText(root, field.key, field.defaultText));
        }

        formLayout.addRow(field.label, editor);
        editors.append({field, editor});
    }

    formWidget.setLayout(&formLayout);
    scrollArea.setWidget(&formWidget);
    scrollArea.setWidgetResizable(true);
    dialogLayout.addWidget(&scrollArea);

    QDialogButtonBox buttonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        &dialog);
    dialogLayout.addWidget(&buttonBox);

    connect(
        &buttonBox,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);
    connect(
        &buttonBox,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    for (const FieldEditor &editor : editors)
    {
        const MasterField &field = editor.field;
        auto *lineEdit = qobject_cast<QLineEdit *>(editor.widget);
        const QString text =
            lineEdit == nullptr ? QString() : lineEdit->text().trimmed();

        if (field.type == MasterFieldType::Int)
        {
            bool ok = false;
            const int parsed = text.toInt(&ok);
            root[field.key] =
                qBound(field.minInt, ok ? parsed : field.defaultInt, field.maxInt);
        }
        else if (field.type == MasterFieldType::Double)
        {
            bool ok = false;
            const double parsed = text.toDouble(&ok);
            root[field.key] =
                qBound(field.minDouble, ok ? parsed : field.defaultDouble, field.maxDouble);
        }
        else
        {
            root[field.key] =
                QColor(text).isValid() ? text : field.defaultText;
        }
    }

    if (!saveMasterJson(root))
    {
        return;
    }

    refreshAfterMasterDataChanged();
    statusBar()->showMessage("マスターデータを保存しました", 2000);
}

void MainWindow::setupActions()
{
    connect(ui->actionScheduleLoad, &QAction::triggered, this, &MainWindow::loadScheduleButton);
    connect(
        ui->actionScheduleSave,
        &QAction::triggered,
        this,
        [this]()
        {
            saveScheduleToFile();
        });
    connect(ui->actionSchedulePrint, &QAction::triggered, this, &MainWindow::showSchedulePrintPreview);

    connect(ui->actionCopyCell, &QAction::triggered, this, &MainWindow::copyCell);
    connect(ui->actionPasteCell, &QAction::triggered, this, &MainWindow::pasteCell);
    connect(ui->actionCutCell, &QAction::triggered, this, &MainWindow::cutCell);
    connect(ui->actionClearCell, &QAction::triggered, this, &MainWindow::clearCell);
    connect(
        ui->actionSelectedCellGuidanceReport,
        &QAction::triggered,
        this,
        &MainWindow::showSelectedCellGuidanceReportPrintPreview);
    connect(ui->actionUndo, &QAction::triggered, this, &MainWindow::undoCellEdit);
    connect(ui->actionRedo, &QAction::triggered, this, &MainWindow::redoCellEdit);

    connect(
        ui->actionEditMasterDays,
        &QAction::triggered,
        this,
        [this]()
        {
            editMasterListValues("days", "曜日");
        });
    connect(
        ui->actionEditMasterPeriods,
        &QAction::triggered,
        this,
        [this]()
        {
            editMasterListValues("periods", "時限");
        });
    connect(
        ui->actionEditMasterGrades,
        &QAction::triggered,
        this,
        [this]()
        {
            editMasterListValues("grades", "学年");
        });
    connect(
        ui->actionEditMasterGenders,
        &QAction::triggered,
        this,
        [this]()
        {
            editMasterListValues("genders", "性別");
        });
    connect(
        ui->actionEditMasterSubjects,
        &QAction::triggered,
        this,
        [this]()
        {
            editMasterListValues("subjects", "教科");
        });
    connect(
        ui->actionEditMasterData,
        &QAction::triggered,
        this,
        &MainWindow::showMasterDataDialog);
}
