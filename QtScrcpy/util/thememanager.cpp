#include "thememanager.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>
#include <QVector>

namespace {
struct ThemeStyleSpec {
    QColor window;
    QColor text;
    QColor base;
    QColor alternateBase;
    QColor button;
    QColor buttonText;
    QColor highlight;
    QColor highlightedText;
    QColor toolTipBase;
    QColor toolTipText;

    QString resourcePrefix;
    QVector<QPair<QString, QString>> replacements;

    QString noVideoText;
    QString noVideoBackground;
    QString localInputBackground;
    QString localInputText;
    QString localInputBorder;
    QString localInputSelection;
    QString keymapPanelBackground;
    QString keymapPanelBorder;
    QString keymapText;
    QString keymapInputBackground;
    QString keymapInputBorder;
    QString keymapButtonBackground;
    QString keymapButtonDisabledText;
    QString keymapSeparator;
};

ThemeStyleSpec buildStyleSpec(bool darkTheme)
{
    ThemeStyleSpec spec;
    if (darkTheme) {
        spec.window = QColor(QStringLiteral("#444444"));
        spec.text = QColor(QStringLiteral("#DCDCDC"));
        spec.base = QColor(QStringLiteral("#383838"));
        spec.alternateBase = QColor(QStringLiteral("#484848"));
        spec.button = QColor(QStringLiteral("#484848"));
        spec.buttonText = QColor(QStringLiteral("#DCDCDC"));
        spec.highlight = QColor(QStringLiteral("#00BB9E"));
        spec.highlightedText = QColor(QStringLiteral("#FFFFFF"));
        spec.toolTipBase = QColor(QStringLiteral("#262626"));
        spec.toolTipText = QColor(QStringLiteral("#F2F2F2"));
        spec.resourcePrefix = QStringLiteral(":/qss/psblack/");
        spec.noVideoText = QStringLiteral("#D0D0D0");
        spec.noVideoBackground = QStringLiteral("#111111");
        spec.localInputBackground = QStringLiteral("rgba(18, 18, 18, 210)");
        spec.localInputText = QStringLiteral("#F2F2F2");
        spec.localInputBorder = QStringLiteral("rgba(255, 255, 255, 90)");
        spec.localInputSelection = QStringLiteral("rgba(64, 128, 255, 180)");
        spec.keymapPanelBackground = QStringLiteral("rgba(24, 24, 24, 228)");
        spec.keymapPanelBorder = QStringLiteral("rgba(255, 255, 255, 40)");
        spec.keymapText = QStringLiteral("#F2F2F2");
        spec.keymapInputBackground = QStringLiteral("rgba(40, 40, 40, 220)");
        spec.keymapInputBorder = QStringLiteral("rgba(255, 255, 255, 50)");
        spec.keymapButtonBackground = QStringLiteral("rgba(70, 70, 70, 220)");
        spec.keymapButtonDisabledText = QStringLiteral("rgba(255,255,255,110)");
        spec.keymapSeparator = QStringLiteral("rgba(255,255,255,40)");
        return spec;
    }

    spec.window = QColor(QStringLiteral("#EEF3F1"));
    spec.text = QColor(QStringLiteral("#24302B"));
    spec.base = QColor(QStringLiteral("#FFFFFF"));
    spec.alternateBase = QColor(QStringLiteral("#F7FAF9"));
    spec.button = QColor(QStringLiteral("#F7FAF9"));
    spec.buttonText = QColor(QStringLiteral("#24302B"));
    spec.highlight = QColor(QStringLiteral("#00BB9E"));
    spec.highlightedText = QColor(QStringLiteral("#FFFFFF"));
    spec.toolTipBase = QColor(QStringLiteral("#FFFFFF"));
    spec.toolTipText = QColor(QStringLiteral("#24302B"));
    spec.resourcePrefix = QStringLiteral(":/qss/pslight/");
    spec.replacements = {
        qMakePair(QStringLiteral("border:1px solid #DCDCDC;"), QStringLiteral("border:1px solid #C7D1CD;")),
        qMakePair(QStringLiteral("rgba(51,127,209,230)"), QStringLiteral("rgba(0,187,158,210)")),
        qMakePair(QStringLiteral("rgba(238,0,0,128)"), QStringLiteral("rgba(230,96,96,180)")),
        qMakePair(QStringLiteral("#264F78"), QStringLiteral("#BFE9E0")),
        qMakePair(QStringLiteral("#DCDCDC"), QStringLiteral("#24302B")),
        qMakePair(QStringLiteral("#646464"), QStringLiteral("#E0E8E5")),
        qMakePair(QStringLiteral("#525252"), QStringLiteral("#D5DFDB")),
        qMakePair(QStringLiteral("#484848"), QStringLiteral("#F7FAF9")),
        qMakePair(QStringLiteral("#444444"), QStringLiteral("#EEF3F1")),
        qMakePair(QStringLiteral("#383838"), QStringLiteral("#E6ECEA")),
        qMakePair(QStringLiteral("#242424"), QStringLiteral("#C7D1CD"))
    };
    spec.noVideoText = QStringLiteral("#24302B");
    spec.noVideoBackground = QStringLiteral("rgba(255, 255, 255, 238)");
    spec.localInputBackground = QStringLiteral("rgba(255, 255, 255, 238)");
    spec.localInputText = QStringLiteral("#24302B");
    spec.localInputBorder = QStringLiteral("rgba(0, 187, 158, 110)");
    spec.localInputSelection = QStringLiteral("rgba(0, 187, 158, 85)");
    spec.keymapPanelBackground = QStringLiteral("rgba(248, 250, 249, 240)");
    spec.keymapPanelBorder = QStringLiteral("rgba(199, 209, 205, 230)");
    spec.keymapText = QStringLiteral("#24302B");
    spec.keymapInputBackground = QStringLiteral("rgba(255, 255, 255, 236)");
    spec.keymapInputBorder = QStringLiteral("rgba(199, 209, 205, 240)");
    spec.keymapButtonBackground = QStringLiteral("rgba(247, 250, 249, 236)");
    spec.keymapButtonDisabledText = QStringLiteral("rgba(36,48,43,110)");
    spec.keymapSeparator = QStringLiteral("rgba(199,209,205,230)");
    return spec;
}

QPalette buildPalette(const ThemeStyleSpec &spec, const QStyle *style)
{
    QPalette palette = style ? style->standardPalette() : QPalette();
    palette.setColor(QPalette::Window, spec.window);
    palette.setColor(QPalette::WindowText, spec.text);
    palette.setColor(QPalette::Base, spec.base);
    palette.setColor(QPalette::AlternateBase, spec.alternateBase);
    palette.setColor(QPalette::Text, spec.text);
    palette.setColor(QPalette::Button, spec.button);
    palette.setColor(QPalette::ButtonText, spec.buttonText);
    palette.setColor(QPalette::Highlight, spec.highlight);
    palette.setColor(QPalette::HighlightedText, spec.highlightedText);
    palette.setColor(QPalette::ToolTipBase, spec.toolTipBase);
    palette.setColor(QPalette::ToolTipText, spec.toolTipText);
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

QString buildApplicationStyleSheet(const ThemeStyleSpec &spec)
{
    QString styleSheet = loadStyleSheet(QStringLiteral(":/qss/themebase.css"));
    if (styleSheet.isEmpty()) {
        return styleSheet;
    }

    if (spec.resourcePrefix != QStringLiteral(":/qss/psblack/")) {
        styleSheet.replace(QStringLiteral(":/qss/psblack/"), spec.resourcePrefix);
        for (const auto &replacement : spec.replacements) {
            styleSheet.replace(replacement.first, replacement.second);
        }
    }
    return styleSheet;
}

QString buildNoVideoOverlayStyleSheet(const ThemeStyleSpec &spec)
{
    return QStringLiteral(
        "QLabel {"
        " color: %1;"
        " background: %2;"
        " font-size: 16px;"
        "}").arg(spec.noVideoText, spec.noVideoBackground);
}

QString buildLocalTextInputOverlayStyleSheet(const ThemeStyleSpec &spec)
{
    return QStringLiteral(
        "QLineEdit {"
        " background: %1;"
        " color: %2;"
        " border: 1px solid %3;"
        " border-radius: 8px;"
        " padding: 6px 10px;"
        " selection-background-color: %4;"
        "}").arg(spec.localInputBackground, spec.localInputText, spec.localInputBorder, spec.localInputSelection);
}

QString buildKeymapEditorPanelStyleSheet(const ThemeStyleSpec &spec)
{
    return QStringLiteral(
        "#keymapEditorPanel {"
        " background: %1;"
        " border: 1px solid %2;"
        " border-radius: 10px;"
        " color: %3;"
        "}"
        "QLabel, QCheckBox, QPushButton { color: %3; }"
        "QLineEdit, QComboBox, QListWidget, QSpinBox {"
        " background: %4;"
        " color: %3;"
        " border: 1px solid %5;"
        " border-radius: 6px;"
        " padding: 4px;"
        "}"
        "QPushButton {"
        " background: %6;"
        " border: 1px solid %5;"
        " border-radius: 6px;"
        " padding: 4px 8px;"
        "}"
        "QPushButton:disabled { color: %7; }"
        "QFrame[themeSeparator=\"true\"] { color: %8; }")
        .arg(spec.keymapPanelBackground,
             spec.keymapPanelBorder,
             spec.keymapText,
             spec.keymapInputBackground,
             spec.keymapInputBorder,
             spec.keymapButtonBackground,
             spec.keymapButtonDisabledText,
             spec.keymapSeparator);
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

QString ThemeManager::noVideoOverlayStyleSheet() const
{
    return buildNoVideoOverlayStyleSheet(buildStyleSpec(m_darkTheme));
}

QString ThemeManager::localTextInputOverlayStyleSheet() const
{
    return buildLocalTextInputOverlayStyleSheet(buildStyleSpec(m_darkTheme));
}

QString ThemeManager::keymapEditorPanelStyleSheet() const
{
    return buildKeymapEditorPanelStyleSheet(buildStyleSpec(m_darkTheme));
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

    const ThemeStyleSpec spec = buildStyleSpec(darkTheme);
    const QString styleSheet = buildApplicationStyleSheet(spec);

    m_app->setPalette(buildPalette(spec, m_app->style()));
    m_app->setStyleSheet(styleSheet);

    const bool changed = (m_darkTheme != darkTheme);
    m_darkTheme = darkTheme;
    if (changed) {
        emit themeChanged();
    }
}
