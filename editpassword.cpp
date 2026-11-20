#include "editpassword.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace
{
enum class Step
{
    Current,
    New,
    Confirm
};

constexpr int minDigits = 4;
constexpr int maxDigits = 8;
const QRegularExpression digitPattern(QStringLiteral("[0-9]*"));

// 現在の照合と新しいパスワードの設定に同じ入力欄を使う。
int runPasswordDialog(QWidget *parent, int currentPassword, bool changing)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(changing ? "パスワードを設定・変更" : "パスワード入力");

    auto *label = new QLabel(&dialog);
    auto *input = new QLineEdit(&dialog);
    input->setEchoMode(QLineEdit::Password);
    input->setInputMethodHints(Qt::ImhDigitsOnly | Qt::ImhSensitiveData | Qt::ImhNoPredictiveText);
    input->setValidator(new QRegularExpressionValidator(digitPattern, input));
    label->setBuddy(input);

    auto *left = new QPushButton(&dialog);
    auto *ok = new QPushButton("OK", &dialog);
    left->setAutoDefault(false);
    ok->setDefault(true);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(left);
    buttons->addStretch();
    buttons->addWidget(ok);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(label);
    layout->addWidget(input);
    layout->addLayout(buttons);

    Step step = currentPassword == 0 ? Step::New : Step::Current;
    int candidate = 0;
    int result = 0;

    const auto showStep = [&](Step next)
    {
        step = next;
        switch (step)
        {
        case Step::Current:
            label->setText("現在のパスワードを入力してください。");
            break;
        case Step::New:
            label->setText("新しいパスワードを入力してください（半角数字4〜8桁）。");
            break;
        case Step::Confirm:
            label->setText("確認のため、新しいパスワードをもう一度入力してください。");
            break;
        }
        left->setText(step == Step::Confirm ? "戻る" : "キャンセル");
        input->clear();
        input->setFocus();
    };

    QObject::connect(left, &QPushButton::clicked, &dialog, [&]()
    {
        if (step == Step::Confirm)
        {
            candidate = 0;
            showStep(Step::New);
            return;
        }
        dialog.reject();
    });

    QObject::connect(ok, &QPushButton::clicked, &dialog, [&]()
    {
        const int entered = EditPassword::encode(input->text());
        if (entered == 0)
        {
            QMessageBox::warning(&dialog, "入力エラー", "パスワードは半角数字4〜8桁で入力してください。");
            input->selectAll();
            input->setFocus();
            return;
        }

        switch (step)
        {
        case Step::Current:
            if (entered != currentPassword)
            {
                QMessageBox::warning(&dialog, "入力エラー", "パスワードが違います。");
                showStep(Step::Current);
            }
            else if (changing)
            {
                showStep(Step::New);
            }
            else
            {
                result = entered;
                dialog.accept();
            }
            break;
        case Step::New:
            candidate = entered;
            showStep(Step::Confirm);
            break;
        case Step::Confirm:
            if (entered != candidate)
            {
                QMessageBox::warning(&dialog, "入力エラー", "新しいパスワードが一致しません。");
                candidate = 0;
                showStep(Step::New);
            }
            else
            {
                result = candidate;
                dialog.accept();
            }
            break;
        }
    });

    dialog.setMinimumWidth(380);
    showStep(step);
    return dialog.exec() == QDialog::Accepted ? result : 0;
}
}

// 未対応の切り替え方式は既定の確認ダイアログに戻す。
int EditPassword::normalizeUnlockMethod(int method)
{
    return method >= NoConfirmation && method <= Password ? method : Confirmation;
}

// 半角数字の桁数を検証し、先頭の0を区別できる整数にする。
int EditPassword::encode(QStringView digits)
{
    if (digits.size() < minDigits || digits.size() > maxDigits)
        return 0;

    int value = 0;
    int place = 1;
    for (QChar digit : digits)
    {
        const int number = digit.unicode() - '0';
        if (number < 0 || number > 9)
            return 0;
        value = value * 10 + number;
        place *= 10;
    }
    return ~(place + value);
    // 桁数を含む整数のビットを反転する。
}

// 保存値が入力可能な範囲に収まることを確認する。
int EditPassword::normalizeStoredValue(int value)
{
    int encoded = ~value;
    // ビットを反転して検証用の整数に戻す。
    if (encoded < 10000 || encoded >= 200000000)
        return 0;

    while (encoded >= 10)
        encoded /= 10;
    return encoded == 1 ? value : 0;
}

// 現在のパスワードが一致したときだけ認証を通す。
bool EditPassword::authenticate(QWidget *parent, int currentPassword)
{
    if (currentPassword == 0)
    {
        QMessageBox::warning(parent, "パスワード未設定", "設定画面でパスワードを設定してください。");
        return false;
    }
    return runPasswordDialog(parent, currentPassword, false) != 0;
}

// 設定中のパスワードがあれば先に照合する。キャンセル時は0を返す。
int EditPassword::change(QWidget *parent, int currentPassword)
{
    return runPasswordDialog(parent, currentPassword, true);
}
