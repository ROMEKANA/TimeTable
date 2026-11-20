#include "editpassword.h"
#include "settingsdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QTimer>
#include <functional>

namespace
{
int failures = 0;

void check(bool passed, const char *name)
{
    if (!passed)
    {
        ++failures;
        QTextStream(stderr) << "FAIL: " << name << '\n';
    }
}

// モーダルダイアログ内の操作を順に実行する。実際の画面や設定ファイルは使わない。
void withDialogSteps(QList<std::function<void(QDialog *)>> steps, const std::function<void()> &action)
{
    QObject context;
    std::function<void()> advance = [&]()
    {
        if (steps.isEmpty())
            return;
        auto step = steps.takeFirst();
        QTimer::singleShot(0, &context, [&advance] { advance(); });
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        check(dialog != nullptr, "expected modal dialog");
        if (dialog != nullptr)
            step(dialog);
    };
    QTimer::singleShot(0, &context, advance);
    action();
    check(steps.isEmpty(), "completed dialog steps");
}

auto enter(const QString &labelText, const QString &digits)
{
    return [=](QDialog *dialog)
    {
        auto *input = dialog->findChild<QLineEdit *>();
        auto *label = dialog->findChild<QLabel *>();
        const bool ready = input != nullptr && label != nullptr && label->text().contains(labelText);
        check(ready, "expected password input stage");
        if (!ready)
        {
            dialog->reject();
            return;
        }
        check(input->echoMode() == QLineEdit::Password, "password is masked");
        input->setText(digits);
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(input, &key);
    };
}

auto click(const QString &text)
{
    return [=](QDialog *dialog)
    {
        for (auto *button : dialog->findChildren<QPushButton *>())
        {
            if (button->text() == text)
            {
                button->click();
                return;
            }
        }
        check(false, "expected dialog button");
        dialog->reject();
    };
}

void dismissWarning(QDialog *dialog)
{
    auto *warning = qobject_cast<QMessageBox *>(dialog);
    check(warning != nullptr && warning->icon() == QMessageBox::Warning, "expected input warning");
    if (warning != nullptr)
        warning->button(QMessageBox::Ok)->click();
    else
        dialog->reject();
}
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    using namespace EditPassword;

    const int original = encode(u"0123");
    const int changed = encode(u"00123");
    check(original == -10124 && changed == -100124, "leading zeroes are preserved");
    check(encode(u"0000") == -10001, "all zeroes");
    check(encode(u"12345678") == -112345679, "eight digits");
    check(encode(u"") == 0 && encode(u"123") == 0 && encode(u"012345678") == 0, "invalid lengths");
    check(encode(u"１２３４") == 0 && encode(u"12a4") == 0 && encode(u" 123") == 0, "ASCII digits only");
    check(normalizeUnlockMethod(0) == 0 && normalizeUnlockMethod(1) == 1 &&
        normalizeUnlockMethod(2) == 2 && normalizeUnlockMethod(3) == Confirmation, "unlock method compatibility");
    check(normalizeStoredValue(original) == original && normalizeStoredValue(changed) == changed &&
        normalizeStoredValue(0) == 0 && normalizeStoredValue(-9999) == 0 &&
        normalizeStoredValue(-23456) == 0, "stored password validation");

    withDialogSteps({
        enter("新しい", "123"), dismissWarning,
        enter("新しい", "123456789"), dismissWarning,
        enter("新しい", "0000"), enter("確認", "0001"), dismissWarning,
        enter("新しい", "0000"), click("戻る"),
        enter("新しい", "0123"), enter("確認", "0123")}, [&]
    {
        check(change(nullptr, 0) == original, "initial setup with retry and back");
    });

    withDialogSteps({
        enter("現在", "1111"), dismissWarning, enter("現在", "0123"),
        enter("新しい", "00123"), enter("確認", "00123")}, [&]
    {
        check(change(nullptr, original) == changed, "password change requires current password");
    });
    withDialogSteps({
        enter("現在", "0123"), dismissWarning, enter("現在", "00123")}, [&]
    {
        check(authenticate(nullptr, changed), "old password rejected, new password accepted");
    });
    withDialogSteps({enter("現在", "0123"), click("キャンセル")}, [&]
    {
        check(change(nullptr, original) == 0, "cancel change after authenticating");
    });

    const QJsonObject legacy{{"scheduleEditConfirmOnUnlock", 0}, {"scheduleEditShowBlockedDialog", 0},
        {"scheduleEditLockedOnStartup", 0}, {"grades", QJsonArray{"G"}}, {"genders", QJsonArray{"X"}},
        {"subjects", QJsonArray{"S"}}, {"customSetting", "keep"}};
    const QStringList days{"月"};
    const QStringList periods{"1限目"};
    SettingsDialog cancelled(legacy, days, periods, false);
    auto *method = cancelled.findChild<QComboBox *>("scheduleEditUnlockMethodComboBox");
    auto *passwordButton = cancelled.findChild<QPushButton *>("scheduleEditPasswordButton");
    check(method->currentIndex() == NoConfirmation && passwordButton->isEnabled(), "legacy settings in viewing mode");
    withDialogSteps({click("キャンセル")}, [&] { method->setCurrentIndex(Password); });
    check(method->currentIndex() == NoConfirmation, "cancel initial setup rolls back selection");
    withDialogSteps({enter("新しい", "0123"), enter("確認", "0123")}, [&] { passwordButton->click(); });
    cancelled.reject();
    check(cancelled.settings() == legacy, "cancel parent discards pending password");

    SettingsDialog initialSetup(legacy, days, periods, false);
    method = initialSetup.findChild<QComboBox *>("scheduleEditUnlockMethodComboBox");
    withDialogSteps({enter("新しい", "0123"), enter("確認", "0123")},
        [&] { method->setCurrentIndex(Password); });
    check(method->currentIndex() == Password, "completed initial setup selects password mode");
    initialSetup.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    const QJsonObject locked = initialSetup.settings();
    check(locked.value("scheduleEditConfirmOnUnlock").toInt() == Password &&
        locked.value("scheduleEditPassword").toInt() == original, "initial setup commits choice and password together");

    SettingsDialog settings(locked, days, periods, false);
    method = settings.findChild<QComboBox *>("scheduleEditUnlockMethodComboBox");
    withDialogSteps({enter("現在", "00123"), dismissWarning, click("キャンセル")},
        [&] { method->setCurrentIndex(Confirmation); });
    check(method->currentIndex() == Password, "failed downgrade leaves password mode selected");
    withDialogSteps({enter("現在", "0123")}, [&] { method->setCurrentIndex(Confirmation); });
    check(method->currentIndex() == Confirmation, "successful downgrade changes selection");
    settings.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    const QJsonObject saved = QJsonDocument::fromJson(QJsonDocument(settings.settings()).toJson()).object();
    check(saved.value("scheduleEditConfirmOnUnlock").toInt(-1) == Confirmation &&
        saved.value("scheduleEditPassword").toInt() == original &&
        saved.value("scheduleEditLockedOnStartup").toInt(-1) == 0 &&
        saved.value("scheduleEditShowBlockedDialog").toInt(-1) == 0 &&
        saved.value("customSetting") == legacy.value("customSetting"), "saved settings round trip and retain existing keys");

    SettingsDialog reloaded(saved, days, periods, true);
    method = reloaded.findChild<QComboBox *>("scheduleEditUnlockMethodComboBox");
    check(method->currentIndex() == Confirmation, "confirmation setting reloads");
    method->setCurrentIndex(Password);
    check(method->currentIndex() == Password, "existing password can be reused");

    return failures == 0 ? 0 : 1;
}
