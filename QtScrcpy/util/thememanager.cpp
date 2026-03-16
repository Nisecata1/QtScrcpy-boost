#include "thememanager.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

namespace {
QPalette buildPalette(bool darkTheme, const QStyle *style)
{
    QPalette palette = style ? style->standardPalette() : QPalette();
    if (darkTheme) {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#444444")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#DCDCDC")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#383838")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#484848")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#DCDCDC")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#484848")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#DCDCDC")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#00BB9E")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#262626")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#F2F2F2")));
    } else {
        palette.setColor(QPalette::Window, QColor(QStringLiteral("#F3F5F7")));
        palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#1F2328")));
        palette.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
        palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#EEF2F6")));
        palette.setColor(QPalette::Text, QColor(QStringLiteral("#1F2328")));
        palette.setColor(QPalette::Button, QColor(QStringLiteral("#FFFFFF")));
        palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#1F2328")));
        palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#0B84F3")));
        palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
        palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#FFFFFF")));
        palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#1F2328")));
    }
    return palette;
}

QString loadStyleSheet(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QFile::ReadOnly)) {
        return QString();
    }
    return QLatin1String(file.readAll());
}
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

ThemeManager &ThemeManager::getInstance()
{
    static ThemeManager manager;
    return manager;
}

void ThemeManager::initialize(QApplication *app)
{
    if (!app) {
        return;
    }

    m_app = app;
    if (!m_initialized) {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
        if (m_app->styleHints()) {
            connect(m_app->styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
                if (configuredMode() == ThemeMode::System) {
                    applyConfiguredTheme();
                }
            });
        }
#endif
        m_initialized = true;
    }

    applyConfiguredTheme();
}

void ThemeManager::applyConfiguredTheme()
{
    applyTheme(resolveDarkTheme(configuredMode()));
}

ThemeMode ThemeManager::configuredMode() const
{
    return Config::getInstance().getUserBootConfig().themeMode;
}

bool ThemeManager::isDarkTheme() const
{
    return m_darkTheme;
}

bool ThemeManager::resolveDarkTheme(ThemeMode mode) const
{
    switch (mode) {
    case ThemeMode::Light:
        return false;
    case ThemeMode::Dark:
        return true;
    case ThemeMode::System:
    default:
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
        if (m_app && m_app->styleHints()) {
            return m_app->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        }
#endif
        return false;
    }
}

void ThemeManager::applyTheme(bool darkTheme)
{
    if (!m_app) {
        return;
    }

    if (m_initialized && m_darkTheme == darkTheme && !m_app->styleSheet().isEmpty()) {
        return;
    }

    const QString styleSheet = loadStyleSheet(darkTheme
        ? QStringLiteral(":/qss/psblack.css")
        : QStringLiteral(":/qss/pslight.css"));

    m_app->setPalette(buildPalette(darkTheme, m_app->style()));
    m_app->setStyleSheet(styleSheet);

    const bool changed = (m_darkTheme != darkTheme);
    m_darkTheme = darkTheme;
    if (changed) {
        emit themeChanged();
    }
}
