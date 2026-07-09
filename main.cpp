#include "mainwindow.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QTranslator>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

namespace
{
#ifdef Q_OS_WIN
    void setRegistryString(
        HKEY root,
        const QString &subKey,
        const QString &valueName,
        const QString &value)
    {
        HKEY key = nullptr;

        if (RegCreateKeyExW(
                root,
                reinterpret_cast<LPCWSTR>(subKey.utf16()),
                0,
                nullptr,
                0,
                KEY_SET_VALUE,
                nullptr,
                &key,
                nullptr) != ERROR_SUCCESS)
        {
            return;
        }

        const LPCWSTR name =
            valueName.isEmpty()
                ? nullptr
                : reinterpret_cast<LPCWSTR>(valueName.utf16());
        const QByteArray data(
            reinterpret_cast<const char *>(value.utf16()),
            (value.size() + 1) * static_cast<int>(sizeof(wchar_t)));

        RegSetValueExW(
            key,
            name,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE *>(data.constData()),
            static_cast<DWORD>(data.size()));
        RegCloseKey(key);
    }

    void registerScheduleFileAssociation()
    {
        const QString appPath =
            QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const QString command =
            QString("\"%1\" \"%2\"").arg(appPath, "%1");

        setRegistryString(
            HKEY_CURRENT_USER,
            "Software\\Classes\\.schedule",
            QString(),
            "TimeTable.schedule");
        setRegistryString(
            HKEY_CURRENT_USER,
            "Software\\Classes\\TimeTable.schedule",
            QString(),
            "TimeTable Schedule");
        setRegistryString(
            HKEY_CURRENT_USER,
            "Software\\Classes\\TimeTable.schedule\\DefaultIcon",
            QString(),
            QString("\"%1\",0").arg(appPath));
        setRegistryString(
            HKEY_CURRENT_USER,
            "Software\\Classes\\TimeTable.schedule\\shell\\open\\command",
            QString(),
            command);

        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
#else
    void registerScheduleFileAssociation()
    {
    }
#endif

    QString startupScheduleFilePath()
    {
        const QStringList arguments = QCoreApplication::arguments();

        for (int i = 1; i < arguments.size(); ++i)
        {
            const QFileInfo fileInfo(arguments[i]);

            if (fileInfo.suffix().compare("schedule", Qt::CaseInsensitive) == 0)
            {
                return fileInfo.absoluteFilePath();
            }
        }

        return QString();
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    registerScheduleFileAssociation();

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "TimeTable_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w(startupScheduleFilePath());
    w.show();
    return QCoreApplication::exec();
}
