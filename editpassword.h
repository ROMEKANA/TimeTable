#ifndef EDITPASSWORD_H
#define EDITPASSWORD_H

#include <QStringView>

class QWidget;

namespace EditPassword
{
enum UnlockMethod
{
    NoConfirmation = 0,
    Confirmation = 1,
    Password = 2
};

int normalizeUnlockMethod(int method);
int encode(QStringView digits);
int normalizeStoredValue(int value);
bool authenticate(QWidget *parent, int currentPassword);
int change(QWidget *parent, int currentPassword);
}

#endif // EDITPASSWORD_H
