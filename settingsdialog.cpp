#include "settingsdialog.h"
#include "ui_settingsdialog.h"

#include <QColor>
#include <QColorDialog>
#include <QDir>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonValue>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>

namespace
{
    // JSON配列から空要素と重複を除いた文字列一覧を取得する
    QStringList stringListFromJson(const QJsonObject &root, const QString &key)
    {
        QStringList values;

        for (const QJsonValue &value : root.value(key).toArray())
        {
            const QString text = value.toString().trimmed();

            if (!text.isEmpty() && !values.contains(text))
            {
                values.append(text);
            }
        }

        return values;
    }

    // 文字列一覧をJSON配列へ変換する
    QJsonArray stringListToJson(const QStringList &values)
    {
        QJsonArray array;

        for (const QString &value : values)
        {
            array.append(value);
        }

        return array;
    }
}

// 現在の設定を読み込んで設定画面を構築する
SettingsDialog::SettingsDialog(
    const QJsonObject &settings,
    const QStringList &days,
    const QStringList &periods,
    bool scheduleStructureEditable,
    QWidget *parent)
    : QDialog(parent),
      ui(new Ui::SettingsDialog),
      sourceSettings(settings),
      resultSettings(settings),
      sourceDays(days),
      sourcePeriods(periods),
      resultDays(days),
      resultPeriods(periods)
{
    ui->setupUi(this);
    ui->scheduleStructureGroupBox->setEnabled(scheduleStructureEditable);
    ui->scheduleStructureNoticeLabel->setVisible(!scheduleStructureEditable);

    loadSettings();
    setupColorButtons();

    connect(
        ui->buttonBox,
        &QDialogButtonBox::accepted,
        this,
        &SettingsDialog::acceptSettings);
    connect(
        ui->buttonBox,
        &QDialogButtonBox::rejected,
        this,
        &QDialog::reject);
    connect(
        ui->schedulePdfOutputDirButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            selectDirectory(
                ui->schedulePdfOutputDirLineEdit,
                "PDFの共通保存先フォルダを選択");
        });
    connect(
        ui->guidanceReportPdfDirButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            selectDirectory(
                ui->guidanceReportPdfDirLineEdit,
                "分割前の指導報告書PDFフォルダを選択");
        });
    connect(
        ui->guidanceReportPdfOutputDirButton,
        &QPushButton::clicked,
        this,
        [this]()
        {
            selectDirectory(
                ui->guidanceReportPdfOutputDirLineEdit,
                "分割後の指導報告書PDF保存先を選択");
        });
}

// UIリソースを解放する
SettingsDialog::~SettingsDialog()
{
    delete ui;
}

// 保存操作で確定した設定を返す
QJsonObject SettingsDialog::settings() const
{
    return resultSettings;
}

// 保存操作で確定した曜日一覧を返す
QStringList SettingsDialog::days() const
{
    return resultDays;
}

// 保存操作で確定した時限一覧を返す
QStringList SettingsDialog::periods() const
{
    return resultPeriods;
}

// 最初に表示する設定タブを選ぶ
void SettingsDialog::setCurrentTab(int tabIndex)
{
    ui->settingsTabWidget->setCurrentIndex(
        qBound(0, tabIndex, ui->settingsTabWidget->count() - 1));
}

// 色選択ボタンと設定キーの対応を返す
QVector<SettingsDialog::ColorField> SettingsDialog::colorFields() const
{
    return {
        {ui->scheduleOddRowColorButton, "scheduleOddRowColor", "#f4f4f4"},
        {ui->scheduleEmptyCellColorButton, "scheduleEmptyCellColor", "#4a4a4a"},
        {ui->scheduleOddRowEmptyCellColorButton, "scheduleOddRowEmptyCellColor", "#3f3f3f"},
        {ui->scheduleOverCapacityCellColorButton, "scheduleOverCapacityCellColor", "#ff6b6b"},
        {ui->scheduleSelectedCellColorButton, "scheduleSelectedCellColor", "#1e3a8a"},
        {ui->scheduleSelectedCellTextColorButton, "scheduleSelectedCellTextColor", "#ffffff"},
        {ui->scheduleTextColorButton, "scheduleTextColor", "#000000"},
        {ui->scheduleOddRowTextColorButton, "scheduleOddRowTextColor", "#000000"},
        {ui->scheduleVerticalLineColorButton, "scheduleVerticalLineColor", "#7d7d7d"},
        {ui->scheduleHorizontalLineColorButton, "scheduleHorizontalLineColor", "#7d7d7d"},
        {ui->scheduleVerticalSectionLineColorButton, "scheduleVerticalSectionLineColor", "#373737"},
        {ui->scheduleHorizontalSectionLineColorButton, "scheduleHorizontalSectionLineColor", "#373737"},
        {ui->guidanceReportTitleColorButton, "guidanceReportTitleColor", "#000000"},
        {ui->guidanceReportInfoColorButton, "guidanceReportInfoColor", "#000000"},
        {ui->guidanceReportOuterLineColorButton, "guidanceReportOuterLineColor", "#000000"},
        {ui->guidanceReportLineColorButton, "guidanceReportLineColor", "#000000"},
        {ui->guidanceReportGridLineColorButton, "guidanceReportGridLineColor", "#cdcdcd"},
        {ui->guidanceReportBoldTextColorButton, "guidanceReportBoldTextColor", "#000000"},
        {ui->guidanceReportTextColorButton, "guidanceReportTextColor", "#000000"}};
}

// JSON設定を各入力欄へ読み込む
void SettingsDialog::loadSettings()
{
    ui->daysPlainTextEdit->setPlainText(sourceDays.join('\n'));
    ui->periodsPlainTextEdit->setPlainText(sourcePeriods.join('\n'));
    ui->gradesPlainTextEdit->setPlainText(stringListFromJson(sourceSettings, "grades").join('\n'));
    ui->gendersPlainTextEdit->setPlainText(stringListFromJson(sourceSettings, "genders").join('\n'));
    ui->subjectsPlainTextEdit->setPlainText(stringListFromJson(sourceSettings, "subjects").join('\n'));

    auto intValue = [this](const char *key, int defaultValue)
    {
        return sourceSettings.value(key).toInt(defaultValue);
    };
    auto textValue = [this](const char *key, const QString &defaultValue)
    {
        const QString text = sourceSettings.value(key).toString(defaultValue).trimmed();
        return text.isEmpty() ? defaultValue : text;
    };

    ui->maxStudentPerTeacherSpinBox->setValue(intValue("MaxStudentPerTeacher", 2));
    ui->cellSectionSizeSpinBox->setValue(intValue("cellSectionSize", 115));
    ui->scheduleDisplayCellHeightSpinBox->setValue(intValue("scheduleDisplayCellHeight", 0));
    ui->scheduleDisplayFontPointSizeSpinBox->setValue(intValue("scheduleDisplayFontPointSize", 12));
    ui->scheduleDisplayHeaderFontPointSizeSpinBox->setValue(intValue("scheduleDisplayHeaderFontPointSize", 10));
    ui->scheduleDisplayHeaderHeightSpinBox->setValue(intValue("scheduleDisplayHeaderHeight", 44));
    ui->scheduleDisplayTimeHeaderWidthSpinBox->setValue(intValue("scheduleDisplayTimeHeaderWidth", 64));
    ui->scrollSpeedDoubleSpinBox->setValue(sourceSettings.value("scrollSpeed").toDouble(0.01));
    ui->scheduleVerticalLineWidthSpinBox->setValue(intValue("scheduleVerticalLineWidth", 1));
    ui->scheduleHorizontalLineWidthSpinBox->setValue(intValue("scheduleHorizontalLineWidth", 1));
    ui->scheduleVerticalSectionLineWidthSpinBox->setValue(intValue("scheduleVerticalSectionLineWidth", 2));
    ui->scheduleHorizontalSectionLineWidthSpinBox->setValue(intValue("scheduleHorizontalSectionLineWidth", 2));
    ui->lessonMemoLookbackWeeksSpinBox->setValue(intValue("lessonMemoLookbackWeeks", 2));
    ui->scheduleEditConfirmOnUnlockCheckBox->setChecked(intValue("scheduleEditConfirmOnUnlock", 1) != 0);
    ui->scheduleEditLockedOnStartupCheckBox->setChecked(intValue("scheduleEditLockedOnStartup", 1) != 0);
    ui->scheduleEditShowBlockedDialogCheckBox->setChecked(intValue("scheduleEditShowBlockedDialog", 1) != 0);
    ui->scheduleEditConfirmOnHistoryClearCheckBox->setChecked(intValue("scheduleEditConfirmOnHistoryClear", 1) != 0);

    ui->schedulePrintDarknessPercentSpinBox->setValue(intValue("schedulePrintDarknessPercent", 115));
    ui->schedulePrintLineWidthPercentSpinBox->setValue(intValue("schedulePrintLineWidthPercent", 100));
    ui->schedulePrintSizePercentSpinBox->setValue(intValue("schedulePrintSizePercent", 96));
    ui->schedulePrintFontPointSizeSpinBox->setValue(intValue("schedulePrintFontPointSize", 9));
    ui->schedulePrintHeaderFontPointSizeSpinBox->setValue(intValue("schedulePrintHeaderFontPointSize", 11));
    ui->schedulePrintTimeColumnPaddingSpinBox->setValue(intValue("schedulePrintTimeColumnPadding", 100));
    ui->schedulePrintDayHeaderHeightSpinBox->setValue(intValue("schedulePrintDayHeaderHeight", 100));
    ui->schedulePrintTeacherHeaderHeightSpinBox->setValue(intValue("schedulePrintTeacherHeaderHeight", 100));
    ui->schedulePrintAutoShrinkTextCheckBox->setChecked(intValue("schedulePrintAutoShrinkText", 0) != 0);
    ui->schedulePdfOutputDirLineEdit->setText(textValue("schedulePdfOutputDir", "schedulePDF"));

    ui->studentHonorificEnabledCheckBox->setChecked(intValue("studentHonorificEnabled", 1) != 0);
    ui->studentHonorificDefaultSuffixLineEdit->setText(textValue("studentHonorificDefaultSuffix", "さん"));
    ui->studentHonorificSpecialGenderLineEdit->setText(textValue("studentHonorificSpecialGender", "男性"));
    ui->studentHonorificSpecialSuffixLineEdit->setText(textValue("studentHonorificSpecialSuffix", "くん"));
    ui->teacherScheduleBlocksPerPageSpinBox->setValue(intValue("teacherScheduleBlocksPerPage", 5));
    ui->teacherScheduleOneLessonPerLineCheckBox->setChecked(intValue("teacherScheduleOneLessonPerLine", 1) != 0);
    ui->teacherScheduleFontPointSizeSpinBox->setValue(intValue("teacherScheduleFontPointSize", 9));
    ui->teacherScheduleIncludeEmptyStudentSlotsCheckBox->setChecked(intValue("teacherScheduleIncludeEmptyStudentSlots", 1) != 0);
    ui->teacherScheduleIncludeEmptySlotsCheckBox->setChecked(intValue("teacherScheduleIncludeEmptySlots", 0) != 0);
    ui->studentSelectionVisibleRowCountSpinBox->setValue(intValue("studentSelectionVisibleRowCount", 10));

    ui->guidanceReportTitleFontPointSizeSpinBox->setValue(intValue("guidanceReportTitleFontPointSize", 15));
    ui->guidanceReportInfoFontPointSizeSpinBox->setValue(intValue("guidanceReportInfoFontPointSize", 18));
    ui->guidanceReportOuterLineWidthSpinBox->setValue(intValue("guidanceReportOuterLineWidth", 3));
    ui->guidanceReportLineWidthSpinBox->setValue(intValue("guidanceReportLineWidth", 1));
    ui->guidanceReportGridLineWidthSpinBox->setValue(intValue("guidanceReportGridLineWidth", 1));
    ui->guidanceReportBoldFontPointSizeSpinBox->setValue(intValue("guidanceReportBoldFontPointSize", 9));
    ui->guidanceReportTextFontPointSizeSpinBox->setValue(intValue("guidanceReportTextFontPointSize", 9));
    ui->guidanceReportPdfDirLineEdit->setText(textValue("guidanceReportPdfDir", "C:/SCAN"));
    ui->guidanceReportPdfOutputDirLineEdit->setText(textValue("guidanceReportPdfOutputDir", "C:/SCAN/分割後"));
    ui->guidanceReportPdfRemoveSpacesFromAutoInputCheckBox->setChecked(intValue("guidanceReportPdfRemoveSpacesFromAutoInput", 1) != 0);

    ui->salaryOneOnTwoRateSpinBox->setValue(intValue("salaryOneOnTwoRate", 2000));
    ui->salaryOneOnOneRateSpinBox->setValue(intValue("salaryOneOnOneRate", 1000));
    ui->salaryHighSchoolAllowanceSpinBox->setValue(intValue("salaryHighSchoolAllowance", 500));
    ui->salaryTransportPaySpinBox->setValue(intValue("salaryTransportPay", 0));
}

// 各入力欄の内容を返却用設定へまとめる
void SettingsDialog::collectSettings()
{
    resultSettings = sourceSettings;
    resultDays = listValues(ui->daysPlainTextEdit);
    resultPeriods = listValues(ui->periodsPlainTextEdit);
    resultSettings["grades"] = stringListToJson(listValues(ui->gradesPlainTextEdit));
    resultSettings["genders"] = stringListToJson(listValues(ui->gendersPlainTextEdit));
    resultSettings["subjects"] = stringListToJson(listValues(ui->subjectsPlainTextEdit));

    resultSettings["MaxStudentPerTeacher"] = ui->maxStudentPerTeacherSpinBox->value();
    resultSettings["cellSectionSize"] = ui->cellSectionSizeSpinBox->value();
    resultSettings["scheduleDisplayCellHeight"] = ui->scheduleDisplayCellHeightSpinBox->value();
    resultSettings["scheduleDisplayFontPointSize"] = ui->scheduleDisplayFontPointSizeSpinBox->value();
    resultSettings["scheduleDisplayHeaderFontPointSize"] = ui->scheduleDisplayHeaderFontPointSizeSpinBox->value();
    resultSettings["scheduleDisplayHeaderHeight"] = ui->scheduleDisplayHeaderHeightSpinBox->value();
    resultSettings["scheduleDisplayTimeHeaderWidth"] = ui->scheduleDisplayTimeHeaderWidthSpinBox->value();
    resultSettings["scrollSpeed"] = ui->scrollSpeedDoubleSpinBox->value();
    resultSettings["scheduleVerticalLineWidth"] = ui->scheduleVerticalLineWidthSpinBox->value();
    resultSettings["scheduleHorizontalLineWidth"] = ui->scheduleHorizontalLineWidthSpinBox->value();
    resultSettings["scheduleVerticalSectionLineWidth"] = ui->scheduleVerticalSectionLineWidthSpinBox->value();
    resultSettings["scheduleHorizontalSectionLineWidth"] = ui->scheduleHorizontalSectionLineWidthSpinBox->value();
    resultSettings["lessonMemoLookbackWeeks"] = ui->lessonMemoLookbackWeeksSpinBox->value();
    resultSettings["scheduleEditConfirmOnUnlock"] = ui->scheduleEditConfirmOnUnlockCheckBox->isChecked() ? 1 : 0;
    resultSettings["scheduleEditLockedOnStartup"] = ui->scheduleEditLockedOnStartupCheckBox->isChecked() ? 1 : 0;
    resultSettings["scheduleEditShowBlockedDialog"] = ui->scheduleEditShowBlockedDialogCheckBox->isChecked() ? 1 : 0;
    resultSettings["scheduleEditConfirmOnHistoryClear"] = ui->scheduleEditConfirmOnHistoryClearCheckBox->isChecked() ? 1 : 0;

    resultSettings["schedulePrintDarknessPercent"] = ui->schedulePrintDarknessPercentSpinBox->value();
    resultSettings["schedulePrintLineWidthPercent"] = ui->schedulePrintLineWidthPercentSpinBox->value();
    resultSettings["schedulePrintSizePercent"] = ui->schedulePrintSizePercentSpinBox->value();
    resultSettings["schedulePrintFontPointSize"] = ui->schedulePrintFontPointSizeSpinBox->value();
    resultSettings["schedulePrintHeaderFontPointSize"] = ui->schedulePrintHeaderFontPointSizeSpinBox->value();
    resultSettings["schedulePrintTimeColumnPadding"] = ui->schedulePrintTimeColumnPaddingSpinBox->value();
    resultSettings["schedulePrintDayHeaderHeight"] = ui->schedulePrintDayHeaderHeightSpinBox->value();
    resultSettings["schedulePrintTeacherHeaderHeight"] = ui->schedulePrintTeacherHeaderHeightSpinBox->value();
    resultSettings["schedulePrintAutoShrinkText"] = ui->schedulePrintAutoShrinkTextCheckBox->isChecked() ? 1 : 0;
    resultSettings["schedulePdfOutputDir"] = ui->schedulePdfOutputDirLineEdit->text().trimmed();

    resultSettings["studentHonorificEnabled"] = ui->studentHonorificEnabledCheckBox->isChecked() ? 1 : 0;
    resultSettings["studentHonorificDefaultSuffix"] = ui->studentHonorificDefaultSuffixLineEdit->text().trimmed();
    resultSettings["studentHonorificSpecialGender"] = ui->studentHonorificSpecialGenderLineEdit->text().trimmed();
    resultSettings["studentHonorificSpecialSuffix"] = ui->studentHonorificSpecialSuffixLineEdit->text().trimmed();
    resultSettings["teacherScheduleBlocksPerPage"] = ui->teacherScheduleBlocksPerPageSpinBox->value();
    resultSettings["teacherScheduleOneLessonPerLine"] = ui->teacherScheduleOneLessonPerLineCheckBox->isChecked() ? 1 : 0;
    resultSettings["teacherScheduleFontPointSize"] = ui->teacherScheduleFontPointSizeSpinBox->value();
    resultSettings["teacherScheduleIncludeEmptyStudentSlots"] = ui->teacherScheduleIncludeEmptyStudentSlotsCheckBox->isChecked() ? 1 : 0;
    resultSettings["teacherScheduleIncludeEmptySlots"] = ui->teacherScheduleIncludeEmptySlotsCheckBox->isChecked() ? 1 : 0;
    resultSettings["studentSelectionVisibleRowCount"] = ui->studentSelectionVisibleRowCountSpinBox->value();

    resultSettings["guidanceReportTitleFontPointSize"] = ui->guidanceReportTitleFontPointSizeSpinBox->value();
    resultSettings["guidanceReportInfoFontPointSize"] = ui->guidanceReportInfoFontPointSizeSpinBox->value();
    resultSettings["guidanceReportOuterLineWidth"] = ui->guidanceReportOuterLineWidthSpinBox->value();
    resultSettings["guidanceReportLineWidth"] = ui->guidanceReportLineWidthSpinBox->value();
    resultSettings["guidanceReportGridLineWidth"] = ui->guidanceReportGridLineWidthSpinBox->value();
    resultSettings["guidanceReportBoldFontPointSize"] = ui->guidanceReportBoldFontPointSizeSpinBox->value();
    resultSettings["guidanceReportTextFontPointSize"] = ui->guidanceReportTextFontPointSizeSpinBox->value();
    resultSettings["guidanceReportPdfDir"] = ui->guidanceReportPdfDirLineEdit->text().trimmed();
    resultSettings["guidanceReportPdfOutputDir"] = ui->guidanceReportPdfOutputDirLineEdit->text().trimmed();
    resultSettings["guidanceReportPdfRemoveSpacesFromAutoInput"] = ui->guidanceReportPdfRemoveSpacesFromAutoInputCheckBox->isChecked() ? 1 : 0;

    resultSettings["salaryOneOnTwoRate"] = ui->salaryOneOnTwoRateSpinBox->value();
    resultSettings["salaryOneOnOneRate"] = ui->salaryOneOnOneRateSpinBox->value();
    resultSettings["salaryHighSchoolAllowance"] = ui->salaryHighSchoolAllowanceSpinBox->value();
    resultSettings["salaryTransportPay"] = ui->salaryTransportPaySpinBox->value();

    for (const ColorField &field : colorFields())
    {
        const QColor color(field.button->property("colorText").toString());
        resultSettings[field.key] =
            color.isValid() ? color.name() : QString(field.defaultColor);
    }
}

// 入力内容を検証して設定を確定する
void SettingsDialog::acceptSettings()
{
    struct RequiredList
    {
        QPlainTextEdit *editor;
        QString label;
    };

    const QVector<RequiredList> requiredLists = {
        {ui->daysPlainTextEdit, "曜日"},
        {ui->periodsPlainTextEdit, "時限"},
        {ui->gradesPlainTextEdit, "学年"},
        {ui->gendersPlainTextEdit, "性別"},
        {ui->subjectsPlainTextEdit, "教科"}};

    for (const RequiredList &required : requiredLists)
    {
        if (!listValues(required.editor).isEmpty())
        {
            continue;
        }

        ui->settingsTabWidget->setCurrentIndex(BasicDataTab);
        required.editor->setFocus();
        QMessageBox::warning(
            this,
            "設定を保存できません",
            required.label + "を1件以上入力してください。");
        return;
    }

    collectSettings();
    QDialog::accept();
}

// 色選択ボタンの表示と操作を設定する
void SettingsDialog::setupColorButtons()
{
    for (const ColorField &field : colorFields())
    {
        const QColor configuredColor(
            sourceSettings.value(field.key).toString(field.defaultColor));
        const QColor initialColor =
            configuredColor.isValid() ? configuredColor : QColor(field.defaultColor);
        updateColorButton(field.button, initialColor);

        connect(
            field.button,
            &QPushButton::clicked,
            this,
            [this, field]()
            {
                const QColor current(field.button->property("colorText").toString());
                const QColor selected = QColorDialog::getColor(
                    current.isValid() ? current : QColor(field.defaultColor),
                    this,
                    "色を選択");

                if (selected.isValid())
                {
                    updateColorButton(field.button, selected);
                }
            });
    }
}

// 色選択ボタンへ現在色を反映する
void SettingsDialog::updateColorButton(QPushButton *button, const QColor &color)
{
    const QString colorText = color.name();
    button->setText(colorText);
    button->setProperty("colorText", colorText);
    button->setStyleSheet(
        QString("background-color: %1; color: %2;")
            .arg(colorText)
            .arg(color.lightness() < 128 ? "#ffffff" : "#000000"));
}

// 対象のフォルダ入力欄を選択ダイアログで更新する
void SettingsDialog::selectDirectory(QLineEdit *lineEdit, const QString &title)
{
    const QString currentPath = lineEdit->text().trimmed();
    const QString selectedPath = QFileDialog::getExistingDirectory(
        this,
        title,
        currentPath);

    if (!selectedPath.isEmpty())
    {
        lineEdit->setText(QDir::fromNativeSeparators(selectedPath));
    }
}

// 複数行入力を空要素と重複のない一覧へ変換する
QStringList SettingsDialog::listValues(const QPlainTextEdit *editor) const
{
    QStringList values;

    for (const QString &line : editor->toPlainText().split('\n'))
    {
        const QString text = line.trimmed();

        if (!text.isEmpty() && !values.contains(text))
        {
            values.append(text);
        }
    }

    return values;
}
