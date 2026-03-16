#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QKeyEvent>
#include <QApplication>
#include <QLocale>
#include <QProcess>
#include <QRandomGenerator>
#include <QTime>
#include <QTimer>
#include <QKeySequenceEdit>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include "config.h"
#include "thememanager.h"
#include "dialog.h"
#include "ui_dialog.h"
#include "videoform.h"
#include "../groupcontroller/groupcontroller.h"

#ifdef Q_OS_WIN32
#include "../util/winutils.h"
#endif

QString s_keyMapPath = "";

const QString &getKeyMapPath()
{
    if (s_keyMapPath.isEmpty()) {
        s_keyMapPath = QString::fromLocal8Bit(qgetenv("QTSCRCPY_KEYMAP_PATH"));
        QFileInfo fileInfo(s_keyMapPath);
        if (s_keyMapPath.isEmpty() || !fileInfo.isDir()) {
            s_keyMapPath = QCoreApplication::applicationDirPath() + "/keymap";
        }
    }
    return s_keyMapPath;
}

Dialog::Dialog(QWidget *parent) : QWidget(parent), ui(new Ui::Widget)
{
    ui->setupUi(this);
    initUI();

    updateBootConfig(true);

    on_useSingleModeCheck_clicked();
    on_updateDevice_clicked();

    connect(&m_autoUpdatetimer, &QTimer::timeout, this, &Dialog::on_updateDevice_clicked);
    if (ui->autoUpdatecheckBox->isChecked()) {
        m_autoUpdatetimer.start(5000);
    }

    connect(&m_adb, &qsc::AdbProcess::adbProcessResult, this, [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
        QString log = "";
        bool newLine = true;
        QStringList args = m_adb.arguments();

        switch (processResult) {
        case qsc::AdbProcess::AER_ERROR_START:
            break;
        case qsc::AdbProcess::AER_SUCCESS_START:
            log = "adb run";
            newLine = false;
            break;
        case qsc::AdbProcess::AER_ERROR_EXEC:
            //log = m_adb.getErrorOut();
            if (args.contains("ifconfig") && args.contains("wlan0")) {
                getIPbyIp();
            }
            break;
        case qsc::AdbProcess::AER_ERROR_MISSING_BINARY:
            log = "adb not found";
            break;
        case qsc::AdbProcess::AER_SUCCESS_EXEC:
            //log = m_adb.getStdOut();
            if (args.contains("devices")) {
                const QString previousSerial = currentSelectedSerial();
                QStringList devices = m_adb.getDevicesSerialFromStdOut();

                int selectedIndex = -1;
                {
                    const QSignalBlocker serialBlocker(ui->serialBox);
                    const QSignalBlocker phoneListBlocker(ui->connectedPhoneList);

                    ui->serialBox->clear();
                    ui->connectedPhoneList->clear();
                    for (auto &item : devices) {
                        ui->serialBox->addItem(item);
                        ui->connectedPhoneList->addItem(Config::getInstance().getNickName(item) + "-" + item);
                    }

                    if (!previousSerial.isEmpty()) {
                        selectedIndex = ui->serialBox->findText(previousSerial);
                    }
                    if (selectedIndex < 0 && ui->serialBox->count() > 0) {
                        selectedIndex = 0;
                    }

                    if (selectedIndex >= 0) {
                        ui->serialBox->setCurrentIndex(selectedIndex);
                        ui->connectedPhoneList->setCurrentRow(selectedIndex);
                    } else {
                        ui->serialBox->setCurrentIndex(-1);
                        ui->connectedPhoneList->setCurrentRow(-1);
                        ui->connectedPhoneList->clearSelection();
                    }
                }

                const QString finalSerial = selectedIndex >= 0
                    ? ui->serialBox->itemText(selectedIndex).trimmed()
                    : QString();
                handleSelectedSerialChanged(finalSerial);
            } else if (args.contains("show") && args.contains("wlan0")) {
                QString ip = m_adb.getDeviceIPFromStdOut();
                if (ip.isEmpty()) {
                    log = "ip not find, connect to wifi?";
                    break;
                }
                ui->deviceIpEdt->setEditText(ip);
            } else if (args.contains("ifconfig") && args.contains("wlan0")) {
                QString ip = m_adb.getDeviceIPFromStdOut();
                if (ip.isEmpty()) {
                    log = "ip not find, connect to wifi?";
                    break;
                }
                ui->deviceIpEdt->setEditText(ip);
            } else if (args.contains("ip -o a")) {
                QString ip = m_adb.getDeviceIPByIpFromStdOut();
                if (ip.isEmpty()) {
                    log = "ip not find, connect to wifi?";
                    break;
                }
                ui->deviceIpEdt->setEditText(ip);
            }
            break;
        }
        if (!log.isEmpty()) {
            outLog(log, newLine);
        }
    });

    m_hideIcon = new QSystemTrayIcon(this);
    m_hideIcon->setIcon(QIcon(":/image/tray/logo.png"));
    m_menu = new QMenu(this);
    m_quit = new QAction(this);
    m_showWindow = new QAction(this);
    m_restart = new QAction(this);
    m_showWindow->setText(tr("show"));
    QString restartText = tr("restart");
    if (restartText == QLatin1String("restart")) {
        const QString configuredLanguage = Config::getInstance().getLanguage();
        if (configuredLanguage == QLatin1String("zh_CN")) {
            restartText = QStringLiteral("重启");
        } else if (configuredLanguage == QLatin1String("ja_JP")) {
            restartText = QStringLiteral("再起動");
        } else if (configuredLanguage == QLatin1String("ko_KR")) {
            restartText = QStringLiteral("다시 시작");
        } else if (configuredLanguage == QLatin1String("Auto")) {
            switch (QLocale().language()) {
            case QLocale::Chinese:
                restartText = QStringLiteral("重启");
                break;
            case QLocale::Japanese:
                restartText = QStringLiteral("再起動");
                break;
            case QLocale::Korean:
                restartText = QStringLiteral("다시 시작");
                break;
            default:
                break;
            }
        }
    }
    m_restart->setText(restartText);
    m_quit->setText(tr("quit"));
    m_menu->addAction(m_showWindow);
    m_menu->addAction(m_restart);
    m_menu->addAction(m_quit);
    m_hideIcon->setContextMenu(m_menu);
    m_hideIcon->show();
    connect(m_showWindow, &QAction::triggered, this, &Dialog::show);
    connect(m_restart, &QAction::triggered, this, &Dialog::restartApplication);
    connect(m_quit, &QAction::triggered, this, [this]() {
        m_hideIcon->hide();
        qApp->quit();
    });
    connect(m_hideIcon, &QSystemTrayIcon::activated, this, &Dialog::slotActivated);

    connect(&qsc::IDeviceManage::getInstance(), &qsc::IDeviceManage::deviceConnected, this, &Dialog::onDeviceConnected);
    connect(&qsc::IDeviceManage::getInstance(), &qsc::IDeviceManage::deviceDisconnected, this, &Dialog::onDeviceDisconnected);
}

Dialog::~Dialog()
{
    qDebug() << "~Dialog()";
    updateBootConfig(false);
    qsc::IDeviceManage::getInstance().disconnectAllDevice();
    delete ui;
}

void Dialog::initUI()
{
    setAttribute(Qt::WA_DeleteOnClose);
    //setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);

    setWindowTitle(Config::getInstance().getTitle());
#ifdef Q_OS_LINUX
    // Set window icon (inherits from application icon set in main.cpp)
    // If application icon was set, this will use it automatically
    if (!qApp->windowIcon().isNull()) {
        setWindowIcon(qApp->windowIcon());
    }
#endif

#ifdef Q_OS_WIN32
    WinUtils::setDarkBorderToWindow((HWND)this->winId(), ThemeManager::getInstance().isDarkTheme());
#endif

    ui->bitRateEdit->setValidator(new QIntValidator(1, 99999, this));

    ui->maxSizeBox->addItem("640");
    ui->maxSizeBox->addItem("720");
    ui->maxSizeBox->addItem("1080");
    ui->maxSizeBox->addItem("1280");
    ui->maxSizeBox->addItem("1920");
    ui->maxSizeBox->addItem(tr("original"));

    ui->formatBox->addItem("mp4");
    ui->formatBox->addItem("mkv");

    ui->lockOrientationBox->addItem(tr("no lock"));
    ui->lockOrientationBox->addItem("0");
    ui->lockOrientationBox->addItem("90");
    ui->lockOrientationBox->addItem("180");
    ui->lockOrientationBox->addItem("270");
    ui->lockOrientationBox->setCurrentIndex(0);

    // 加载IP历史记录
    loadIpHistory();

    // 加载端口历史记录
    loadPortHistory();

    // 为deviceIpEdt添加右键菜单
    if (ui->deviceIpEdt->lineEdit()) {
        ui->deviceIpEdt->lineEdit()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->deviceIpEdt->lineEdit(), &QWidget::customContextMenuRequested,
                this, &Dialog::showIpEditMenu);
    }
    
    // 为devicePortEdt添加右键菜单
    if (ui->devicePortEdt->lineEdit()) {
        ui->devicePortEdt->lineEdit()->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(ui->devicePortEdt->lineEdit(), &QWidget::customContextMenuRequested,
                this, &Dialog::showPortEditMenu);
    }

    auto configLayout = qobject_cast<QVBoxLayout *>(ui->configGroupBox->layout());
    if (configLayout) {
        auto *themeWidget = new QWidget(ui->configGroupBox);
        auto *themeLayout = new QHBoxLayout(themeWidget);
        themeLayout->setContentsMargins(0, 0, 0, 0);

        auto *themeLabel = new QLabel(tr("主题模式："), themeWidget);
        m_themeModeBox = new QComboBox(themeWidget);
        m_themeModeBox->addItem(tr("跟随系统"), static_cast<int>(ThemeMode::System));
        m_themeModeBox->addItem(tr("浅色"), static_cast<int>(ThemeMode::Light));
        m_themeModeBox->addItem(tr("深色"), static_cast<int>(ThemeMode::Dark));
        m_themeModeBox->setToolTip(tr("切换整个应用的主题外观，可选择跟随系统、浅色或深色。"));
        themeLabel->setBuddy(m_themeModeBox);

        themeLayout->addWidget(themeLabel);
        themeLayout->addWidget(m_themeModeBox);
        themeLayout->addStretch(1);
        configLayout->insertWidget(0, themeWidget);
    }

    connect(ui->serialBox, &QComboBox::currentTextChanged,
            this, &Dialog::handleSelectedSerialChanged);
    connect(ui->maxFpsSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::onGlobalMaxFpsValueChanged);
    if (m_themeModeBox) {
        connect(m_themeModeBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                this, &Dialog::onThemeModeChanged);
    }
    connect(&ThemeManager::getInstance(), &ThemeManager::themeChanged, this, [this]() {
#ifdef Q_OS_WIN32
        WinUtils::setDarkBorderToWindow((HWND)this->winId(), ThemeManager::getInstance().isDarkTheme());
#endif
    });

    initSelectedDeviceConfigUi();
    handleSelectedSerialChanged(currentSelectedSerial());
}

void Dialog::initSelectedDeviceConfigUi()
{
    if (m_selectedDeviceConfigGroup) {
        return;
    }

    auto rightLayout = qobject_cast<QVBoxLayout *>(ui->rightWidget->layout());
    if (!rightLayout) {
        return;
    }

    m_selectedDeviceConfigGroup = new QGroupBox(tr("设备独有板块"), ui->rightWidget);
    m_selectedDeviceConfigGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto groupLayout = new QVBoxLayout(m_selectedDeviceConfigGroup);
    groupLayout->setContentsMargins(5, 5, 5, 5);
    groupLayout->setSpacing(4);

    auto addLabeledMouseConfigRow = [this](QFormLayout *layout, const QString &labelText,
                                           const QString &toolTip, QWidget *field) {
        auto *label = new QLabel(labelText, m_mouseConfigContent);
        label->setToolTip(toolTip);
        field->setToolTip(toolTip);
        layout->addRow(label, field);
    };

    auto selectedDeviceRow = new QHBoxLayout();
    selectedDeviceRow->setContentsMargins(0, 0, 0, 0);
    selectedDeviceRow->addWidget(new QLabel(tr("当前设备："), m_selectedDeviceConfigGroup));
    m_selectedDeviceSerialValue = new QLabel(tr("未选择设备"), m_selectedDeviceConfigGroup);
    m_selectedDeviceSerialValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    selectedDeviceRow->addWidget(m_selectedDeviceSerialValue, 1);
    groupLayout->addLayout(selectedDeviceRow);

    auto deviceCenterCropRow = new QHBoxLayout();
    deviceCenterCropRow->setContentsMargins(0, 0, 0, 0);
    m_deviceCenterCropCheck = new QCheckBox(tr("center crop"), m_selectedDeviceConfigGroup);
    deviceCenterCropRow->addWidget(m_deviceCenterCropCheck);
    deviceCenterCropRow->addStretch(1);

    auto deviceCenterCropLabel = new QLabel(tr("crop size:"), m_selectedDeviceConfigGroup);
    deviceCenterCropRow->addWidget(deviceCenterCropLabel);

    m_deviceCenterCropSizeSpin = new QSpinBox(m_selectedDeviceConfigGroup);
    m_deviceCenterCropSizeSpin->setRange(2, 4096);
    m_deviceCenterCropSizeSpin->setSingleStep(2);
    m_deviceCenterCropSizeSpin->setValue(256);
    m_deviceCenterCropSizeSpin->setEnabled(false);
    deviceCenterCropLabel->setBuddy(m_deviceCenterCropSizeSpin);
    deviceCenterCropRow->addWidget(m_deviceCenterCropSizeSpin);
    groupLayout->addLayout(deviceCenterCropRow);

    auto deviceMaxFpsRow = new QHBoxLayout();
    deviceMaxFpsRow->setContentsMargins(0, 0, 0, 0);
    auto deviceMaxFpsLabel = new QLabel(tr("最大帧率："), m_selectedDeviceConfigGroup);
    deviceMaxFpsRow->addWidget(deviceMaxFpsLabel);

    m_deviceMaxFpsSpin = new QSpinBox(m_selectedDeviceConfigGroup);
    m_deviceMaxFpsSpin->setRange(0, 240);
    m_deviceMaxFpsSpin->setToolTip(tr("0 = 不限制，重启服务后生效"));
    deviceMaxFpsLabel->setBuddy(m_deviceMaxFpsSpin);
    deviceMaxFpsRow->addWidget(m_deviceMaxFpsSpin);
    deviceMaxFpsRow->addStretch(1);

    m_deviceMaxFpsOverrideCheck = new QCheckBox(tr("启用独有配置"), m_selectedDeviceConfigGroup);
    deviceMaxFpsRow->addWidget(m_deviceMaxFpsOverrideCheck);
    groupLayout->addLayout(deviceMaxFpsRow);

    m_mouseConfigToggleBtn = new QToolButton(m_selectedDeviceConfigGroup);
    m_mouseConfigToggleBtn->setText(tr("鼠标显示设置"));
    m_mouseConfigToggleBtn->setCheckable(true);
    m_mouseConfigToggleBtn->setChecked(false);
    m_mouseConfigToggleBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_mouseConfigToggleBtn->setArrowType(Qt::RightArrow);
    connect(m_mouseConfigToggleBtn, &QToolButton::toggled, this, &Dialog::setMouseConfigExpanded);
    groupLayout->addWidget(m_mouseConfigToggleBtn);

    m_mouseConfigContent = new QWidget(m_selectedDeviceConfigGroup);
    auto mouseLayout = new QFormLayout(m_mouseConfigContent);
    mouseLayout->setContentsMargins(18, 0, 0, 0);
    mouseLayout->setSpacing(4);

    const QString remoteCursorToolTip = tr("控制手机画面里的远端黑色鼠标光标是否渲染；关闭后会按现有隐藏逻辑清掉。");
    const QString cursorSizeToolTip = tr("远端光标大小，范围 8~128。");
    const QString normalMouseCompatToolTip = tr("非相对视角下的普通鼠标兼容总开关；关闭时下面几项兼容参数不生效。");
    const QString touchPriorityToolTip = tr("让普通点击和滑动触控优先于远端黑光标刷新。");
    const QString cursorThrottleToolTip = tr("启用普通模式远端黑光标限频与点击静默；关闭后恢复更即时的光标发送。");
    const QString flushIntervalToolTip = tr("黑光标刷新间隔，范围 16~100 ms；越大越稳，但越不跟手。");
    const QString clickSuppressionToolTip = tr("点击前后黑光标静默时间，范围 0~300 ms；越大越优先保证点击。");
    const QString tapMinHoldToolTip = tr("普通左键轻点最小按压时长，范围 0~40 ms；0 表示关闭，越大越稳但点击感更慢。");

    m_renderRemoteCursorCheck = new QCheckBox(m_mouseConfigContent);
    addLabeledMouseConfigRow(mouseLayout, tr("Render Remote Cursor"), remoteCursorToolTip, m_renderRemoteCursorCheck);

    m_cursorSizeSpin = new QSpinBox(m_mouseConfigContent);
    m_cursorSizeSpin->setRange(8, 128);
    addLabeledMouseConfigRow(mouseLayout, tr("Cursor Size Px"), cursorSizeToolTip, m_cursorSizeSpin);

    m_normalMouseCompatEnabledCheck = new QCheckBox(m_mouseConfigContent);
    addLabeledMouseConfigRow(mouseLayout, tr("Normal Mouse Compat"), normalMouseCompatToolTip, m_normalMouseCompatEnabledCheck);

    m_normalMouseTouchPriorityEnabledCheck = new QCheckBox(m_mouseConfigContent);
    addLabeledMouseConfigRow(mouseLayout, tr("Touch Priority"), touchPriorityToolTip, m_normalMouseTouchPriorityEnabledCheck);

    m_normalMouseCursorThrottleEnabledCheck = new QCheckBox(m_mouseConfigContent);
    addLabeledMouseConfigRow(mouseLayout, tr("Cursor Throttle"), cursorThrottleToolTip, m_normalMouseCursorThrottleEnabledCheck);

    m_normalMouseCursorFlushIntervalSpin = new QSpinBox(m_mouseConfigContent);
    m_normalMouseCursorFlushIntervalSpin->setRange(16, 100);
    m_normalMouseCursorFlushIntervalSpin->setSuffix(tr(" ms"));
    addLabeledMouseConfigRow(mouseLayout, tr("Cursor Flush Interval"), flushIntervalToolTip, m_normalMouseCursorFlushIntervalSpin);

    m_normalMouseCursorClickSuppressionSpin = new QSpinBox(m_mouseConfigContent);
    m_normalMouseCursorClickSuppressionSpin->setRange(0, 300);
    m_normalMouseCursorClickSuppressionSpin->setSuffix(tr(" ms"));
    addLabeledMouseConfigRow(mouseLayout, tr("Click Suppression"), clickSuppressionToolTip, m_normalMouseCursorClickSuppressionSpin);

    m_normalMouseTapMinHoldSpin = new QSpinBox(m_mouseConfigContent);
    m_normalMouseTapMinHoldSpin->setRange(0, 40);
    m_normalMouseTapMinHoldSpin->setSuffix(tr(" ms"));
    addLabeledMouseConfigRow(mouseLayout, tr("Tap Min Hold"), tapMinHoldToolTip, m_normalMouseTapMinHoldSpin);

    groupLayout->addWidget(m_mouseConfigContent);
    setMouseConfigExpanded(false);

    connect(m_renderRemoteCursorCheck, &QCheckBox::toggled, this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_normalMouseCompatEnabledCheck, &QCheckBox::toggled, this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_normalMouseTouchPriorityEnabledCheck, &QCheckBox::toggled, this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_normalMouseCursorThrottleEnabledCheck, &QCheckBox::toggled, this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_cursorSizeSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_normalMouseCursorFlushIntervalSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_normalMouseCursorClickSuppressionSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_normalMouseTapMinHoldSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::onSelectedDeviceMouseConfigEdited);
    connect(m_deviceMaxFpsSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::onSelectedDeviceMaxFpsConfigEdited);
    connect(m_deviceMaxFpsOverrideCheck, &QCheckBox::toggled,
            this, &Dialog::onSelectedDeviceMaxFpsConfigEdited);
    connect(m_deviceCenterCropCheck, &QCheckBox::toggled,
            this, &Dialog::onSelectedDeviceCenterCropConfigEdited);
    connect(m_deviceCenterCropSizeSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &Dialog::onSelectedDeviceCenterCropConfigEdited);

    rightLayout->insertWidget(1, m_selectedDeviceConfigGroup);
}

QString Dialog::currentSelectedSerial() const
{
    if (!ui || !ui->serialBox) {
        return QString();
    }
    return ui->serialBox->currentText().trimmed();
}

ThemeMode Dialog::currentThemeModeSelection() const
{
    if (!m_themeModeBox) {
        return ThemeMode::System;
    }

    const QVariant value = m_themeModeBox->currentData();
    if (!value.isValid()) {
        return ThemeMode::System;
    }

    return static_cast<ThemeMode>(value.toInt());
}

QString Dialog::formatSelectedDeviceDisplayName(const QString &serial) const
{
    const QString trimmedSerial = serial.trimmed();
    if (trimmedSerial.isEmpty()) {
        return tr("未选择设备");
    }

    const QString nickName = Config::getInstance().getNickName(trimmedSerial).trimmed();
    if (nickName.isEmpty() || nickName == QLatin1String("Phone")) {
        return trimmedSerial;
    }

    return QStringLiteral("%1 - %2").arg(nickName, trimmedSerial);
}

void Dialog::setMouseConfigExpanded(bool expanded)
{
    if (!m_mouseConfigToggleBtn || !m_mouseConfigContent) {
        return;
    }

    m_mouseConfigToggleBtn->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    m_mouseConfigContent->setVisible(expanded);
}

void Dialog::updateSelectedDeviceConfigControlState()
{
    if (!m_selectedDeviceConfigGroup) {
        return;
    }

    const bool hasSerial = !currentSelectedSerial().isEmpty();
    const bool centerCropEnabled = hasSerial
        && m_deviceCenterCropCheck
        && m_deviceCenterCropCheck->isChecked();
    const bool deviceMaxFpsOverrideEnabled = hasSerial
        && m_deviceMaxFpsOverrideCheck
        && m_deviceMaxFpsOverrideCheck->isChecked();
    const bool compatEnabled = hasSerial
        && m_normalMouseCompatEnabledCheck
        && m_normalMouseCompatEnabledCheck->isChecked();

    m_selectedDeviceConfigGroup->setEnabled(hasSerial);
    if (m_deviceCenterCropCheck) {
        m_deviceCenterCropCheck->setEnabled(hasSerial);
    }
    if (m_deviceCenterCropSizeSpin) {
        m_deviceCenterCropSizeSpin->setEnabled(centerCropEnabled);
    }
    if (m_deviceMaxFpsOverrideCheck) {
        m_deviceMaxFpsOverrideCheck->setEnabled(hasSerial);
    }
    if (m_deviceMaxFpsSpin) {
        m_deviceMaxFpsSpin->setEnabled(deviceMaxFpsOverrideEnabled);
    }
    if (m_renderRemoteCursorCheck) {
        m_renderRemoteCursorCheck->setEnabled(hasSerial);
    }
    if (m_cursorSizeSpin) {
        m_cursorSizeSpin->setEnabled(hasSerial);
    }
    if (m_normalMouseCompatEnabledCheck) {
        m_normalMouseCompatEnabledCheck->setEnabled(hasSerial);
    }
    if (m_normalMouseTouchPriorityEnabledCheck) {
        m_normalMouseTouchPriorityEnabledCheck->setEnabled(compatEnabled);
    }
    if (m_normalMouseCursorThrottleEnabledCheck) {
        m_normalMouseCursorThrottleEnabledCheck->setEnabled(compatEnabled);
    }
    if (m_normalMouseCursorFlushIntervalSpin) {
        m_normalMouseCursorFlushIntervalSpin->setEnabled(compatEnabled);
    }
    if (m_normalMouseCursorClickSuppressionSpin) {
        m_normalMouseCursorClickSuppressionSpin->setEnabled(compatEnabled);
    }
    if (m_normalMouseTapMinHoldSpin) {
        m_normalMouseTapMinHoldSpin->setEnabled(compatEnabled);
    }
}

void Dialog::updateSelectedDeviceConfigUi(const QString &serial)
{
    if (!m_selectedDeviceConfigGroup) {
        return;
    }

    const QString trimmedSerial = serial.trimmed();
    m_updatingSelectedDeviceConfigUi = true;

    if (m_selectedDeviceSerialValue) {
        m_selectedDeviceSerialValue->setText(formatSelectedDeviceDisplayName(trimmedSerial));
    }

    const bool centerCropEnabled = Config::getInstance().isDeviceCenterCropEnabled(trimmedSerial);
    const int centerCropSize = centerCropEnabled
        ? Config::getInstance().getDeviceCenterCropSize(trimmedSerial)
        : 256;
    const bool hasMaxFpsOverride = Config::getInstance().hasDeviceMaxFpsOverride(trimmedSerial);
    const int fallbackMaxFps = ui->maxFpsSpin ? ui->maxFpsSpin->value() : Config::getInstance().getGlobalMaxFps();
    const int deviceMaxFps = hasMaxFpsOverride
        ? Config::getInstance().getDeviceMaxFpsOverride(trimmedSerial)
        : fallbackMaxFps;
    DeviceMouseConfig config = Config::getInstance().getDeviceMouseConfig(trimmedSerial);

    const QSignalBlocker centerCropCheckBlocker(m_deviceCenterCropCheck);
    const QSignalBlocker centerCropSizeBlocker(m_deviceCenterCropSizeSpin);
    const QSignalBlocker deviceMaxFpsBlocker(m_deviceMaxFpsSpin);
    const QSignalBlocker deviceMaxFpsOverrideBlocker(m_deviceMaxFpsOverrideCheck);
    const QSignalBlocker remoteCursorBlocker(m_renderRemoteCursorCheck);
    const QSignalBlocker cursorSizeBlocker(m_cursorSizeSpin);
    const QSignalBlocker compatBlocker(m_normalMouseCompatEnabledCheck);
    const QSignalBlocker touchPriorityBlocker(m_normalMouseTouchPriorityEnabledCheck);
    const QSignalBlocker throttleBlocker(m_normalMouseCursorThrottleEnabledCheck);
    const QSignalBlocker flushBlocker(m_normalMouseCursorFlushIntervalSpin);
    const QSignalBlocker suppressionBlocker(m_normalMouseCursorClickSuppressionSpin);
    const QSignalBlocker tapHoldBlocker(m_normalMouseTapMinHoldSpin);

    if (m_deviceCenterCropCheck) {
        m_deviceCenterCropCheck->setChecked(centerCropEnabled);
    }
    if (m_deviceCenterCropSizeSpin) {
        m_deviceCenterCropSizeSpin->setValue(centerCropSize);
    }
    if (m_deviceMaxFpsOverrideCheck) {
        m_deviceMaxFpsOverrideCheck->setChecked(hasMaxFpsOverride);
    }
    if (m_deviceMaxFpsSpin) {
        m_deviceMaxFpsSpin->setValue(deviceMaxFps);
    }
    if (m_renderRemoteCursorCheck) {
        m_renderRemoteCursorCheck->setChecked(config.remoteCursorEnabled);
    }
    if (m_cursorSizeSpin) {
        m_cursorSizeSpin->setValue(config.cursorSizePx);
    }
    if (m_normalMouseCompatEnabledCheck) {
        m_normalMouseCompatEnabledCheck->setChecked(config.normalMouseCompatEnabled);
    }
    if (m_normalMouseTouchPriorityEnabledCheck) {
        m_normalMouseTouchPriorityEnabledCheck->setChecked(config.normalMouseTouchPriorityEnabled);
    }
    if (m_normalMouseCursorThrottleEnabledCheck) {
        m_normalMouseCursorThrottleEnabledCheck->setChecked(config.normalMouseCursorThrottleEnabled);
    }
    if (m_normalMouseCursorFlushIntervalSpin) {
        m_normalMouseCursorFlushIntervalSpin->setValue(config.normalMouseCursorFlushIntervalMs);
    }
    if (m_normalMouseCursorClickSuppressionSpin) {
        m_normalMouseCursorClickSuppressionSpin->setValue(config.normalMouseCursorClickSuppressionMs);
    }
    if (m_normalMouseTapMinHoldSpin) {
        m_normalMouseTapMinHoldSpin->setValue(config.normalMouseTapMinHoldMs);
    }

    updateSelectedDeviceConfigControlState();
    m_updatingSelectedDeviceConfigUi = false;
}

void Dialog::saveSelectedDeviceMouseConfig()
{
    if (m_updatingSelectedDeviceConfigUi) {
        return;
    }

    const QString serial = currentSelectedSerial();
    if (serial.isEmpty()) {
        updateSelectedDeviceConfigControlState();
        return;
    }

    DeviceMouseConfig config;
    config.remoteCursorEnabled = m_renderRemoteCursorCheck && m_renderRemoteCursorCheck->isChecked();
    config.cursorSizePx = m_cursorSizeSpin ? m_cursorSizeSpin->value() : 24;
    config.normalMouseCompatEnabled = m_normalMouseCompatEnabledCheck && m_normalMouseCompatEnabledCheck->isChecked();
    config.normalMouseTouchPriorityEnabled = m_normalMouseTouchPriorityEnabledCheck
        && m_normalMouseTouchPriorityEnabledCheck->isChecked();
    config.normalMouseCursorThrottleEnabled = m_normalMouseCursorThrottleEnabledCheck
        && m_normalMouseCursorThrottleEnabledCheck->isChecked();
    config.normalMouseCursorFlushIntervalMs = m_normalMouseCursorFlushIntervalSpin
        ? m_normalMouseCursorFlushIntervalSpin->value() : 33;
    config.normalMouseCursorClickSuppressionMs = m_normalMouseCursorClickSuppressionSpin
        ? m_normalMouseCursorClickSuppressionSpin->value() : 120;
    config.normalMouseTapMinHoldMs = m_normalMouseTapMinHoldSpin
        ? m_normalMouseTapMinHoldSpin->value() : 16;

    Config::getInstance().setDeviceMouseConfig(serial, config);
    updateSelectedDeviceConfigControlState();
}

void Dialog::onSelectedDeviceMouseConfigEdited()
{
    saveSelectedDeviceMouseConfig();
}

void Dialog::saveSelectedDeviceCenterCropConfig()
{
    if (m_updatingSelectedDeviceConfigUi) {
        return;
    }

    const QString serial = currentSelectedSerial();
    if (serial.isEmpty()) {
        updateSelectedDeviceConfigControlState();
        return;
    }

    const bool enabled = m_deviceCenterCropCheck && m_deviceCenterCropCheck->isChecked();
    if (enabled) {
        const int cropSize = m_deviceCenterCropSizeSpin ? m_deviceCenterCropSizeSpin->value() : 256;
        Config::getInstance().setDeviceCenterCropSize(serial, cropSize);
    } else {
        Config::getInstance().clearDeviceCenterCropSize(serial);
        if (m_deviceCenterCropSizeSpin) {
            const QSignalBlocker blocker(m_deviceCenterCropSizeSpin);
            m_deviceCenterCropSizeSpin->setValue(256);
        }
    }

    updateSelectedDeviceConfigControlState();
}

void Dialog::onSelectedDeviceCenterCropConfigEdited()
{
    saveSelectedDeviceCenterCropConfig();
}

void Dialog::saveSelectedDeviceMaxFpsConfig()
{
    if (m_updatingSelectedDeviceConfigUi) {
        return;
    }

    const QString serial = currentSelectedSerial();
    if (serial.isEmpty()) {
        updateSelectedDeviceConfigControlState();
        return;
    }

    const bool useDeviceOverride = m_deviceMaxFpsOverrideCheck && m_deviceMaxFpsOverrideCheck->isChecked();
    if (useDeviceOverride) {
        const int maxFps = m_deviceMaxFpsSpin ? m_deviceMaxFpsSpin->value() : 0;
        Config::getInstance().setDeviceMaxFpsOverride(serial, maxFps);
    } else {
        Config::getInstance().clearDeviceMaxFpsOverride(serial);
        if (m_deviceMaxFpsSpin && ui->maxFpsSpin) {
            const QSignalBlocker blocker(m_deviceMaxFpsSpin);
            m_deviceMaxFpsSpin->setValue(ui->maxFpsSpin->value());
        }
    }

    updateSelectedDeviceConfigControlState();
}

void Dialog::onSelectedDeviceMaxFpsConfigEdited()
{
    saveSelectedDeviceMaxFpsConfig();
}

void Dialog::onGlobalMaxFpsValueChanged(int value)
{
    Q_UNUSED(value);
    if (!m_deviceMaxFpsSpin || !m_deviceMaxFpsOverrideCheck || m_deviceMaxFpsOverrideCheck->isChecked()) {
        return;
    }

    const QSignalBlocker blocker(m_deviceMaxFpsSpin);
    m_deviceMaxFpsSpin->setValue(ui->maxFpsSpin->value());
}

void Dialog::updateBootConfig(bool toView)
{
    if (toView) {
        UserBootConfig config = Config::getInstance().getUserBootConfig();

        if (config.bitRate == 0) {
            ui->bitRateBox->setCurrentText("Mbps");
        } else if (config.bitRate % 1000000 == 0) {
            ui->bitRateEdit->setText(QString::number(config.bitRate / 1000000));
            ui->bitRateBox->setCurrentText("Mbps");
        } else {
            ui->bitRateEdit->setText(QString::number(config.bitRate / 1000));
            ui->bitRateBox->setCurrentText("Kbps");
        }

        if (m_themeModeBox) {
            const QSignalBlocker blocker(m_themeModeBox);
            const int index = m_themeModeBox->findData(static_cast<int>(config.themeMode));
            m_themeModeBox->setCurrentIndex(index >= 0 ? index : 0);
        }
        ui->maxFpsSpin->setValue(config.maxFps);
        ui->maxSizeBox->setCurrentIndex(config.maxSizeIndex);
        ui->formatBox->setCurrentIndex(config.recordFormatIndex);
        ui->recordPathEdt->setText(config.recordPath);
        ui->lockOrientationBox->setCurrentIndex(config.lockOrientationIndex);
        ui->localTextInputCheck->setChecked(config.localTextInputEnabled);
        ui->localTextInputShortcutEdit->setKeySequence(QKeySequence::fromString(config.localTextInputShortcut, QKeySequence::PortableText));
        ui->framelessCheck->setChecked(config.framelessWindow);
        ui->recordScreenCheck->setChecked(config.recordScreen);
        ui->notDisplayCheck->setChecked(config.recordBackground);
        ui->useReverseCheck->setChecked(config.reverseConnect);
        ui->fpsCheck->setChecked(config.showFPS);
        ui->alwaysTopCheck->setChecked(config.windowOnTop);
        ui->closeScreenCheck->setChecked(config.autoOffScreen);
        ui->stayAwakeCheck->setChecked(config.keepAlive);
        ui->useSingleModeCheck->setChecked(config.simpleMode);
        ui->autoUpdatecheckBox->setChecked(config.autoUpdateDevice);
        ui->showToolbar->setChecked(config.showToolbar);
        applyLocalTextInputConfigToOpenVideoForms();
    } else {
        UserBootConfig config;

        config.bitRate = getBitRate();
        config.themeMode = currentThemeModeSelection();
        config.maxFps = ui->maxFpsSpin->value();
        config.maxSizeIndex = ui->maxSizeBox->currentIndex();
        config.recordFormatIndex = ui->formatBox->currentIndex();
        config.recordPath = ui->recordPathEdt->text();
        config.lockOrientationIndex = ui->lockOrientationBox->currentIndex();
        config.localTextInputEnabled = ui->localTextInputCheck->isChecked();
        config.localTextInputShortcut = ui->localTextInputShortcutEdit->keySequence().toString(QKeySequence::PortableText);
        config.recordScreen = ui->recordScreenCheck->isChecked();
        config.recordBackground = ui->notDisplayCheck->isChecked();
        config.reverseConnect = ui->useReverseCheck->isChecked();
        config.showFPS = ui->fpsCheck->isChecked();
        config.windowOnTop = ui->alwaysTopCheck->isChecked();
        config.autoOffScreen = ui->closeScreenCheck->isChecked();
        config.framelessWindow = ui->framelessCheck->isChecked();
        config.keepAlive = ui->stayAwakeCheck->isChecked();
        config.simpleMode = ui->useSingleModeCheck->isChecked();
        config.autoUpdateDevice = ui->autoUpdatecheckBox->isChecked();
        config.showToolbar = ui->showToolbar->isChecked();

        // 保存当前IP到历史记录
        QString currentIp = ui->deviceIpEdt->currentText().trimmed();
        if (!currentIp.isEmpty()) {
            saveIpHistory(currentIp);
        }

        Config::getInstance().setUserBootConfig(config);
    }
}

void Dialog::onThemeModeChanged(int index)
{
    Q_UNUSED(index);
    UserBootConfig config = Config::getInstance().getUserBootConfig();
    config.themeMode = currentThemeModeSelection();
    Config::getInstance().setUserBootConfig(config);
    ThemeManager::getInstance().applyConfiguredTheme();
}

void Dialog::execAdbCmd()
{
    if (checkAdbRun()) {
        return;
    }
    QString cmd = ui->adbCommandEdt->text().trimmed();
    outLog("adb " + cmd, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    m_adb.execute(ui->serialBox->currentText().trimmed(), cmd.split(" ", Qt::SkipEmptyParts));
#else
    m_adb.execute(ui->serialBox->currentText().trimmed(), cmd.split(" ", QString::SkipEmptyParts));
#endif
}

void Dialog::delayMs(int ms)
{
    QTime dieTime = QTime::currentTime().addMSecs(ms);

    while (QTime::currentTime() < dieTime) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }
}

QString Dialog::getGameScript(const QString &fileName)
{
    if (fileName.isEmpty()) {
        return "";
    }

    QFile loadFile(getGameScriptPath(fileName));
    if (!loadFile.open(QIODevice::ReadOnly)) {
        outLog("open file failed:" + fileName, true);
        return "";
    }

    QString ret = loadFile.readAll();
    loadFile.close();
    return ret;
}

QString Dialog::getGameScriptPath(const QString &fileName) const
{
    if (fileName.isEmpty()) {
        return QString();
    }
    return getKeyMapPath() + "/" + fileName;
}

void Dialog::slotActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
#ifdef Q_OS_WIN32
        this->show();
#endif
        break;
    default:
        break;
    }
}

void Dialog::closeEvent(QCloseEvent *event)
{
    this->hide();
    if (!Config::getInstance().getTrayMessageShown()) {
        Config::getInstance().setTrayMessageShown(true);
        m_hideIcon->showMessage(tr("Notice"),
                                tr("Hidden here!"),
                                QSystemTrayIcon::Information,
                                3000);
    }
    event->ignore();
}

void Dialog::on_updateDevice_clicked()
{
    if (checkAdbRun()) {
        return;
    }
    outLog("update devices...", false);
    m_adb.execute("", QStringList() << "devices");
}

void Dialog::on_startServerBtn_clicked()
{
    updateBootConfig(false);
    outLog("start server...", false);
    qsc::IDeviceManage::getInstance().connectDevice(buildDeviceParams(ui->serialBox->currentText().trimmed()));
}

void Dialog::on_stopServerBtn_clicked()
{
    if (qsc::IDeviceManage::getInstance().disconnectDevice(ui->serialBox->currentText().trimmed())) {
        outLog("stop server");
    }
}

void Dialog::on_restartServerBtn_clicked()
{
    onRestartDeviceRequested(currentSelectedSerial());
}

void Dialog::on_wirelessConnectBtn_clicked()
{
    if (checkAdbRun()) {
        return;
    }
    QString addr = ui->deviceIpEdt->currentText().trimmed();
    if (addr.isEmpty()) {
        outLog("error: device ip is null", false);
        return;
    }

    if (!ui->devicePortEdt->currentText().isEmpty()) {
        addr += ":";
        addr += ui->devicePortEdt->currentText().trimmed();
    } else if (!ui->devicePortEdt->lineEdit()->placeholderText().isEmpty()) {
        addr += ":";
        addr += ui->devicePortEdt->lineEdit()->placeholderText().trimmed();
    } else {
        outLog("error: device port is null", false);
        return;
    }

    // 保存IP历史记录 - 只保存IP部分,不包含端口
    QString ip = addr.split(":").first();
    if (!ip.isEmpty()) {
        saveIpHistory(ip);
    }
    
    // 保存端口历史记录
    QString port = addr.split(":").last();
    if (!port.isEmpty() && port != ip) {
        savePortHistory(port);
    }

    outLog("wireless connect...", false);
    QStringList adbArgs;
    adbArgs << "connect";
    adbArgs << addr;
    m_adb.execute("", adbArgs);
}

void Dialog::on_startAdbdBtn_clicked()
{
    if (checkAdbRun()) {
        return;
    }
    outLog("start devices adbd...", false);
    // adb tcpip 5555
    QStringList adbArgs;
    adbArgs << "tcpip";
    adbArgs << "5555";
    m_adb.execute(ui->serialBox->currentText().trimmed(), adbArgs);
}

void Dialog::outLog(const QString &log, bool newLine)
{
    // avoid sub thread update ui
    QString backLog = log;
    QTimer::singleShot(0, this, [this, backLog, newLine]() {
        ui->outEdit->append(backLog);
        if (newLine) {
            ui->outEdit->append("<br/>");
        }
    });
}

bool Dialog::filterLog(const QString &log)
{
    if (log.contains("app_proces")) {
        return true;
    }
    if (log.contains("Unable to set geometry")) {
        return true;
    }
    return false;
}

bool Dialog::checkAdbRun()
{
    if (m_adb.isRuning()) {
        outLog("wait for the end of the current command to run");
    }
    return m_adb.isRuning();
}

void Dialog::on_getIPBtn_clicked()
{
    if (checkAdbRun()) {
        return;
    }

    outLog("get ip...", false);
    // adb -s P7C0218510000537 shell ifconfig wlan0
    // or
    // adb -s P7C0218510000537 shell ip -f inet addr show wlan0
    QStringList adbArgs;
#if 0
    adbArgs << "shell";
    adbArgs << "ip";
    adbArgs << "-f";
    adbArgs << "inet";
    adbArgs << "addr";
    adbArgs << "show";
    adbArgs << "wlan0";
#else
    adbArgs << "shell";
    adbArgs << "ifconfig";
    adbArgs << "wlan0";
#endif
    m_adb.execute(ui->serialBox->currentText().trimmed(), adbArgs);
}

void Dialog::getIPbyIp()
{
    if (checkAdbRun()) {
        return;
    }

    QStringList adbArgs;
    adbArgs << "shell";
    adbArgs << "ip -o a";

    m_adb.execute(ui->serialBox->currentText().trimmed(), adbArgs);
}

void Dialog::onDeviceConnected(bool success, const QString &serial, const QString &deviceName, const QSize &size, int initialOrientation)
{
    Q_UNUSED(deviceName);
    if (!success) {
        return;
    }
    auto videoForm = new VideoForm(ui->framelessCheck->isChecked(), Config::getInstance().getSkin(), ui->showToolbar->isChecked());
    videoForm->setSerial(serial);
    videoForm->setInitialOrientationHint(initialOrientation);
    videoForm->setLocalTextInputConfig(ui->localTextInputCheck->isChecked(), ui->localTextInputShortcutEdit->keySequence());
    connect(videoForm, &VideoForm::restartServiceRequested, this, &Dialog::onRestartDeviceRequested);
    connect(videoForm, &QObject::destroyed, this, [this, serial]() {
        m_videoForms.remove(serial);
    });
    m_videoForms.insert(serial, videoForm);
    updateVideoFormScriptBinding(serial, getGameScriptPath(ui->gameBox->currentText()), ui->gameBox->currentText(), getGameScript(ui->gameBox->currentText()));

    qsc::IDeviceManage::getInstance().getDevice(serial)->setUserData(static_cast<void*>(videoForm));
    qsc::IDeviceManage::getInstance().getDevice(serial)->registerDeviceObserver(videoForm);


    videoForm->showFPS(ui->fpsCheck->isChecked());

    if (ui->alwaysTopCheck->isChecked()) {
        videoForm->staysOnTop();
    }

#ifndef Q_OS_WIN32
    // must be show before updateShowSize
    videoForm->show();
#endif
    QString name = Config::getInstance().getNickName(serial);
    if (name.isEmpty()) {
        name = Config::getInstance().getTitle();
    }
    videoForm->setWindowTitle(name + "-" + serial);
    videoForm->updateShowSize(size);

    bool deviceVer = size.height() > size.width();
    QRect rc = Config::getInstance().getRect(serial);
    bool rcVer = rc.height() > rc.width();
    // same width/height rate
    if (rc.isValid() && (deviceVer == rcVer)) {
        // mark: resize is for fix setGeometry magneticwidget bug
        videoForm->resize(rc.size());
        videoForm->setGeometry(rc);
    }

#ifdef Q_OS_WIN32
    // windows是show太早可以看到resize的过程
    QTimer::singleShot(200, videoForm, [videoForm](){videoForm->show();});
#endif

    GroupController::instance().addDevice(serial);
}

void Dialog::onRestartDeviceRequested(const QString &serial)
{
    const QString trimmedSerial = serial.trimmed();
    if (trimmedSerial.isEmpty()) {
        outLog("restart server failed: serial is empty");
        return;
    }

    updateBootConfig(false);
    outLog(QString("restart server: %1").arg(trimmedSerial), false);

    if (!qsc::IDeviceManage::getInstance().disconnectDevice(trimmedSerial)) {
        outLog(QString("restart server failed: device not found (%1)").arg(trimmedSerial));
        return;
    }

    QTimer::singleShot(0, this, [this, trimmedSerial]() {
        const qsc::DeviceParams params = buildDeviceParams(trimmedSerial);
        if (params.serial.trimmed().isEmpty()) {
            outLog("restart server failed: serial is empty");
            return;
        }
        if (!qsc::IDeviceManage::getInstance().connectDevice(params)) {
            outLog(QString("restart server failed: connect device failed (%1)").arg(trimmedSerial));
        }
    });
}

void Dialog::onDeviceDisconnected(QString serial)
{
    m_videoForms.remove(serial);
    GroupController::instance().removeDevice(serial);
    auto device = qsc::IDeviceManage::getInstance().getDevice(serial);
    if (!device) {
        return;
    }
    auto data = device->getUserData();
    if (data) {
        VideoForm* vf = static_cast<VideoForm*>(data);
        qsc::IDeviceManage::getInstance().getDevice(serial)->deRegisterDeviceObserver(vf);
        vf->close();
        vf->deleteLater();
    }
}

void Dialog::on_wirelessDisConnectBtn_clicked()
{
    if (checkAdbRun()) {
        return;
    }
    QString addr = ui->deviceIpEdt->currentText().trimmed();
    outLog("wireless disconnect...", false);
    QStringList adbArgs;
    adbArgs << "disconnect";
    adbArgs << addr;
    m_adb.execute("", adbArgs);
}

void Dialog::on_selectRecordPathBtn_clicked()
{
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString directory = QFileDialog::getExistingDirectory(this, tr("select path"), "", options);
    ui->recordPathEdt->setText(directory);
}

void Dialog::on_openAppFolderBtn_clicked()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(appDir))) {
        outLog(QString("open app folder: %1").arg(appDir));
        return;
    }

    outLog(QString("could not open app folder: %1").arg(appDir));
    QMessageBox::warning(this, tr("open app folder"), tr("Could not open application folder:\n%1").arg(appDir));
}

void Dialog::on_recordPathEdt_textChanged(const QString &arg1)
{
    ui->recordPathEdt->setToolTip(arg1.trimmed());
    ui->notDisplayCheck->setCheckable(!arg1.trimmed().isEmpty());
}

void Dialog::on_adbCommandBtn_clicked()
{
    execAdbCmd();
}

void Dialog::on_stopAdbBtn_clicked()
{
    m_adb.kill();
}

void Dialog::on_clearOut_clicked()
{
    ui->outEdit->clear();
}

void Dialog::on_stopAllServerBtn_clicked()
{
    qsc::IDeviceManage::getInstance().disconnectAllDevice();
}

void Dialog::on_refreshGameScriptBtn_clicked()
{
    ui->gameBox->clear();
    QDir dir(getKeyMapPath());
    if (!dir.exists()) {
        outLog("keymap directory not find", true);
        return;
    }
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    QFileInfoList list = dir.entryInfoList();
    QFileInfo fileInfo;
    int size = list.size();
    for (int i = 0; i < size; ++i) {
        fileInfo = list.at(i);
        ui->gameBox->addItem(fileInfo.fileName());
    }
}

void Dialog::on_applyScriptBtn_clicked()
{
    auto curSerial = ui->serialBox->currentText().trimmed();
    auto device = qsc::IDeviceManage::getInstance().getDevice(curSerial);
    if (!device) {
        return;
    }

    const QString scriptJson = getGameScript(ui->gameBox->currentText());
    device->updateScript(scriptJson);
    updateVideoFormScriptBinding(curSerial, getGameScriptPath(ui->gameBox->currentText()), ui->gameBox->currentText(), scriptJson);
}

void Dialog::on_recordScreenCheck_clicked(bool checked)
{
    if (!checked) {
        return;
    }

    QString fileDir(ui->recordPathEdt->text().trimmed());
    if (fileDir.isEmpty()) {
        qWarning() << "please select record save path!!!";
        ui->recordScreenCheck->setChecked(false);
    }
}

void Dialog::on_localTextInputCheck_toggled(bool checked)
{
    ui->localTextInputShortcutEdit->setEnabled(checked);
    applyLocalTextInputConfigToOpenVideoForms();
}

void Dialog::on_localTextInputShortcutEdit_keySequenceChanged(const QKeySequence &keySequence)
{
    Q_UNUSED(keySequence);
    applyLocalTextInputConfigToOpenVideoForms();
}

void Dialog::on_usbConnectBtn_clicked()
{
    on_stopAllServerBtn_clicked();
    delayMs(200);
    on_updateDevice_clicked();
    delayMs(200);

    int firstUsbDevice = findDeviceFromeSerialBox(false);
    if (-1 == firstUsbDevice) {
        qWarning() << "No use device is found!";
        return;
    }
    ui->serialBox->setCurrentIndex(firstUsbDevice);

    on_startServerBtn_clicked();
}

int Dialog::findDeviceFromeSerialBox(bool wifi)
{
    QString regStr = "\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\:([0-9]|[1-9]\\d|[1-9]\\d{2}|[1-9]\\d{3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])\\b";
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QRegExp regIP(regStr);
#else
    QRegularExpression regIP(regStr);
#endif
    for (int i = 0; i < ui->serialBox->count(); ++i) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        bool isWifi = regIP.exactMatch(ui->serialBox->itemText(i));
#else
        bool isWifi = regIP.match(ui->serialBox->itemText(i)).hasMatch();
#endif
        bool found = wifi ? isWifi : !isWifi;
        if (found) {
            return i;
        }
    }

    return -1;
}

void Dialog::on_wifiConnectBtn_clicked()
{
    on_stopAllServerBtn_clicked();
    delayMs(200);

    on_updateDevice_clicked();
    delayMs(200);

    int firstUsbDevice = findDeviceFromeSerialBox(false);
    if (-1 == firstUsbDevice) {
        qWarning() << "No use device is found!";
        return;
    }
    ui->serialBox->setCurrentIndex(firstUsbDevice);

    on_getIPBtn_clicked();
    delayMs(200);

    on_startAdbdBtn_clicked();
    delayMs(1000);

    on_wirelessConnectBtn_clicked();
    delayMs(2000);

    on_updateDevice_clicked();
    delayMs(200);

    int firstWifiDevice = findDeviceFromeSerialBox(true);
    if (-1 == firstWifiDevice) {
        qWarning() << "No wifi device is found!";
        return;
    }
    ui->serialBox->setCurrentIndex(firstWifiDevice);

    on_startServerBtn_clicked();
}

void Dialog::on_connectedPhoneList_itemDoubleClicked(QListWidgetItem *item)
{
    Q_UNUSED(item);
    ui->serialBox->setCurrentIndex(ui->connectedPhoneList->currentRow());
    on_startServerBtn_clicked();
}

void Dialog::on_updateNameBtn_clicked()
{
    if (ui->serialBox->count() != 0) {
        if (ui->userNameEdt->text().isEmpty()) {
            Config::getInstance().setNickName(ui->serialBox->currentText(), "Phone");
        } else {
            Config::getInstance().setNickName(ui->serialBox->currentText(), ui->userNameEdt->text());
        }

        handleSelectedSerialChanged(currentSelectedSerial());

        on_updateDevice_clicked();

        qDebug() << "Update OK!";
    } else {
        qWarning() << "No device is connected!";
    }
}

void Dialog::on_useSingleModeCheck_clicked()
{
    if (ui->useSingleModeCheck->isChecked()) {
        ui->rightWidget->hide();
    } else {
        ui->rightWidget->show();
    }

    adjustSize();
}

void Dialog::handleSelectedSerialChanged(const QString &serial)
{
    ui->userNameEdt->setText(Config::getInstance().getNickName(serial));
    updateSelectedDeviceConfigUi(serial);
}

quint32 Dialog::getBitRate()
{
    return ui->bitRateEdit->text().trimmed().toUInt() *
            (ui->bitRateBox->currentText() == QString("Mbps") ? 1000000 : 1000);
}

qsc::DeviceParams Dialog::buildDeviceParams(const QString &serial)
{
    // this is ok that "original" toUshort is 0
    quint16 videoSize = ui->maxSizeBox->currentText().trimmed().toUShort();
    const QString trimmedSerial = serial.trimmed();
    qsc::DeviceParams params;
    params.serial = trimmedSerial;
    params.maxSize = videoSize;
    params.bitRate = getBitRate();
    params.maxFps = static_cast<quint32>(Config::getInstance().getEffectiveMaxFps(trimmedSerial));
    params.closeScreen = ui->closeScreenCheck->isChecked();
    params.useReverse = ui->useReverseCheck->isChecked();
    params.display = !ui->notDisplayCheck->isChecked();
    params.renderExpiredFrames = Config::getInstance().getRenderExpiredFrames();
    if (ui->lockOrientationBox->currentIndex() > 0) {
        params.captureOrientationLock = 1;
        params.captureOrientation = (ui->lockOrientationBox->currentIndex() - 1) * 90;
    }
    params.stayAwake = ui->stayAwakeCheck->isChecked();
    params.recordFile = ui->recordScreenCheck->isChecked();
    params.recordPath = ui->recordPathEdt->text().trimmed();
    params.recordFileFormat = ui->formatBox->currentText().trimmed();
    params.serverLocalPath = getServerPath();
    params.serverRemotePath = Config::getInstance().getServerPath();
    params.pushFilePath = Config::getInstance().getPushFilePath();
    params.gameScript = getGameScript(ui->gameBox->currentText());
    params.logLevel = Config::getInstance().getLogLevel();
    params.codecOptions = Config::getInstance().getCodecOptions();
    params.codecName = Config::getInstance().getCodecName();
    params.scid = QRandomGenerator::global()->bounded(1, 10000) & 0x7FFFFFFF;
    return params;
}

void Dialog::applyLocalTextInputConfigToOpenVideoForms()
{
    const bool enabled = ui->localTextInputCheck->isChecked();
    const QKeySequence shortcut = ui->localTextInputShortcutEdit->keySequence();
    for (auto it = m_videoForms.begin(); it != m_videoForms.end();) {
        if (it.value().isNull()) {
            it = m_videoForms.erase(it);
            continue;
        }
        it.value()->setLocalTextInputConfig(enabled, shortcut);
        ++it;
    }
}

void Dialog::updateVideoFormScriptBinding(const QString &serial, const QString &scriptFilePath, const QString &scriptDisplayName, const QString &scriptJson)
{
    auto it = m_videoForms.find(serial);
    if (it == m_videoForms.end() || it.value().isNull()) {
        return;
    }
    it.value()->setScriptBinding(scriptFilePath, scriptDisplayName, scriptJson);
}

const QString &Dialog::getServerPath()
{
    static QString serverPath;
    if (serverPath.isEmpty()) {
        serverPath = QString::fromLocal8Bit(qgetenv("QTSCRCPY_SERVER_PATH"));
        QFileInfo fileInfo(serverPath);
        if (serverPath.isEmpty() || !fileInfo.isFile()) {
            serverPath = QCoreApplication::applicationDirPath() + "/scrcpy-server";
        }
    }
    return serverPath;
}

void Dialog::on_startAudioBtn_clicked()
{
    if (ui->serialBox->count() == 0) {
        qWarning() << "No device is connected!";
        return;
    }

    m_audioOutput.start(ui->serialBox->currentText(), 28200);
}

void Dialog::on_stopAudioBtn_clicked()
{
    m_audioOutput.stop();
}

void Dialog::on_installSndcpyBtn_clicked()
{
    if (ui->serialBox->count() == 0) {
        qWarning() << "No device is connected!";
        return;
    }
    m_audioOutput.installonly(ui->serialBox->currentText(), 28200);
}

void Dialog::on_autoUpdatecheckBox_toggled(bool checked)
{
    if (checked) {
        m_autoUpdatetimer.start(5000);
    } else {
        m_autoUpdatetimer.stop();
    }
}

void Dialog::loadIpHistory()
{
    QStringList ipList = Config::getInstance().getIpHistory();
    ui->deviceIpEdt->clear();
    ui->deviceIpEdt->addItems(ipList);
    ui->deviceIpEdt->setContentsMargins(0, 0, 0, 0);

    if (ui->deviceIpEdt->lineEdit()) {
        ui->deviceIpEdt->lineEdit()->setMaxLength(128);
        ui->deviceIpEdt->lineEdit()->setPlaceholderText("192.168.0.1");
    }
}

void Dialog::saveIpHistory(const QString &ip)
{
    if (ip.isEmpty()) {
        return;
    }
    
    Config::getInstance().saveIpHistory(ip);
    
    // 更新ComboBox
    loadIpHistory();
    ui->deviceIpEdt->setCurrentText(ip);
}

void Dialog::showIpEditMenu(const QPoint &pos)
{
    QMenu *menu = ui->deviceIpEdt->lineEdit()->createStandardContextMenu();
    menu->addSeparator();
    
    QAction *clearHistoryAction = new QAction(tr("Clear History"), menu);
    connect(clearHistoryAction, &QAction::triggered, this, [this]() {
        Config::getInstance().clearIpHistory();
        loadIpHistory();
    });
    
    menu->addAction(clearHistoryAction);
    menu->exec(ui->deviceIpEdt->lineEdit()->mapToGlobal(pos));
    delete menu;
}

void Dialog::loadPortHistory()
{
    QStringList portList = Config::getInstance().getPortHistory();
    ui->devicePortEdt->clear();
    ui->devicePortEdt->addItems(portList);
    ui->devicePortEdt->setContentsMargins(0, 0, 0, 0);

    if (ui->devicePortEdt->lineEdit()) {
        ui->devicePortEdt->lineEdit()->setMaxLength(6);
        ui->devicePortEdt->lineEdit()->setPlaceholderText("5555");
    }
}

void Dialog::savePortHistory(const QString &port)
{
    if (port.isEmpty()) {
        return;
    }
    
    Config::getInstance().savePortHistory(port);
    
    // 更新ComboBox
    loadPortHistory();
    ui->devicePortEdt->setCurrentText(port);
}

void Dialog::showPortEditMenu(const QPoint &pos)
{
    QMenu *menu = ui->devicePortEdt->lineEdit()->createStandardContextMenu();
    menu->addSeparator();
    
    QAction *clearHistoryAction = new QAction(tr("Clear History"), menu);
    connect(clearHistoryAction, &QAction::triggered, this, [this]() {
        Config::getInstance().clearPortHistory();
        loadPortHistory();
    });
    
    menu->addAction(clearHistoryAction);
    menu->exec(ui->devicePortEdt->lineEdit()->mapToGlobal(pos));
    delete menu;
}

void Dialog::restartApplication()
{
    const QString program = QCoreApplication::applicationFilePath();
    const QStringList args = QCoreApplication::arguments().mid(1);
    const QString workdir = QCoreApplication::applicationDirPath();

    if (QProcess::startDetached(program, args, workdir)) {
        outLog(QString("restart application: %1").arg(program));
        m_hideIcon->hide();
        qApp->quit();
        return;
    }

    const QString error = QString("restart application failed: %1").arg(program);
    outLog(error);
    QMessageBox::warning(this, tr("restart"), error);
}
