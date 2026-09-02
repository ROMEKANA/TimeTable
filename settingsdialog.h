#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QStringList>
#include <QVector>

class QPushButton;
class QColor;
class QLineEdit;
class QPlainTextEdit;

namespace Ui
{
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum TabIndex
    {
        BasicDataTab = 0,
        ScheduleTab = 1,
        ScheduleOutputTab = 2,
        GuidanceReportTab = 3,
        SalaryTab = 4
    };

    explicit SettingsDialog(const QJsonObject &settings, const QStringList &days, const QStringList &periods, bool scheduleStructureEditable, QWidget *parent = nullptr); // 現在の設定を読み込んで設定画面を構築する
    ~SettingsDialog() override; // UIリソースを解放する
    QJsonObject settings() const; // 保存操作で確定した設定を返す
    QStringList days() const; // 保存操作で確定した曜日一覧を返す
    QStringList periods() const; // 保存操作で確定した時限一覧を返す
    void setCurrentTab(int tabIndex); // 最初に表示する設定タブを選ぶ

private:
    struct ColorField
    {
        QPushButton *button;
        const char *key;
        const char *defaultColor;
    };

    Ui::SettingsDialog *ui;
    QJsonObject sourceSettings;
    QJsonObject resultSettings;
    QStringList sourceDays;
    QStringList sourcePeriods;
    QStringList resultDays;
    QStringList resultPeriods;

    QVector<ColorField> colorFields() const; // 色選択ボタンと設定キーの対応を返す
    void loadSettings(); // JSON設定を各入力欄へ読み込む
    void collectSettings(); // 各入力欄の内容を返却用設定へまとめる
    void acceptSettings(); // 入力内容を検証して設定を確定する
    void setupColorButtons(); // 色選択ボタンの表示と操作を設定する
    void updateColorButton(QPushButton *button, const QColor &color); // 色選択ボタンへ現在色を反映する
    void selectDirectory(QLineEdit *lineEdit, const QString &title); // 対象のフォルダ入力欄を選択ダイアログで更新する
    QStringList listValues(const QPlainTextEdit *editor) const; // 複数行入力を空要素と重複のない一覧へ変換する
};

#endif // SETTINGSDIALOG_H
