#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>

#include "config.h"

class QApplication;

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager &getInstance();

    void initialize(QApplication *app);
    void applyConfiguredTheme();
    ThemeMode configuredMode() const;
    bool isDarkTheme() const;

signals:
    void themeChanged();

private:
    explicit ThemeManager(QObject *parent = nullptr);
    bool resolveDarkTheme(ThemeMode mode) const;
    void applyTheme(bool darkTheme);

private:
    QApplication *m_app = nullptr;
    bool m_initialized = false;
    bool m_darkTheme = false;
};

#endif // THEMEMANAGER_H
