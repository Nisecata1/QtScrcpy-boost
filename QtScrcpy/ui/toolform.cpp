#include <QDebug>
#include <QHideEvent>
#include <QMouseEvent>
#include <QShowEvent>

#include "iconhelper.h"
#include "toolform.h"
#include "ui_toolform.h"
#include "videoform.h"
#include "../groupcontroller/groupcontroller.h"

ToolForm::ToolForm(QWidget *adsorbWidget, AdsorbPositions adsorbPos) : MagneticWidget(adsorbWidget, adsorbPos), ui(new Ui::ToolForm)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    //setWindowFlags(windowFlags() & ~Qt::WindowMinMaxButtonsHint);

    updateGroupControl();

    initStyle();
    setRecordingState(false);
}

ToolForm::~ToolForm()
{
    delete ui;
}

void ToolForm::setSerial(const QString &serial)
{
    m_serial = serial;
}

void ToolForm::setRecordingState(bool active, const QString &filePath)
{
    m_recordingActive = active;
    m_recordingFilePath = filePath;

    ui->startRecordBtn->setEnabled(!m_recordingActive);
    ui->stopRecordBtn->setEnabled(m_recordingActive);
    updateToolTips();
}

bool ToolForm::isHost()
{
    return m_isHost;
}

void ToolForm::initStyle()
{
    IconHelper::Instance()->SetIcon(ui->fullScreenBtn, QChar(0xf0b2), 15);
    IconHelper::Instance()->SetIcon(ui->restartServiceBtn, QChar(0xf021), 15);
    IconHelper::Instance()->SetIcon(ui->startRecordBtn, QChar(0xf04b), 15);
    IconHelper::Instance()->SetIcon(ui->stopRecordBtn, QChar(0xf04d), 15);
    IconHelper::Instance()->SetIcon(ui->menuBtn, QChar(0xf096), 15);
    IconHelper::Instance()->SetIcon(ui->homeBtn, QChar(0xf1db), 15);
    //IconHelper::Instance()->SetIcon(ui->returnBtn, QChar(0xf104), 15);
    IconHelper::Instance()->SetIcon(ui->returnBtn, QChar(0xf053), 15);
    IconHelper::Instance()->SetIcon(ui->appSwitchBtn, QChar(0xf24d), 15);
    IconHelper::Instance()->SetIcon(ui->volumeUpBtn, QChar(0xf028), 15);
    IconHelper::Instance()->SetIcon(ui->volumeDownBtn, QChar(0xf027), 15);
    IconHelper::Instance()->SetIcon(ui->openScreenBtn, QChar(0xf06e), 15);
    IconHelper::Instance()->SetIcon(ui->closeScreenBtn, QChar(0xf070), 15);
    IconHelper::Instance()->SetIcon(ui->powerBtn, QChar(0xf011), 15);
    IconHelper::Instance()->SetIcon(ui->expandNotifyBtn, QChar(0xf103), 15);
    IconHelper::Instance()->SetIcon(ui->screenShotBtn, QChar(0xf0c4), 15);
    IconHelper::Instance()->SetIcon(ui->touchBtn, QChar(0xf111), 15);
    IconHelper::Instance()->SetIcon(ui->groupControlBtn, QChar(0xf0c0), 15);
    IconHelper::Instance()->SetIcon(ui->clipboardBtn, QChar(0xf0c5), 15);
}

void ToolForm::updateToolTips()
{
    ui->fullScreenBtn->setToolTip(tr("切换当前视频窗口的全屏显示状态。"));
    ui->restartServiceBtn->setToolTip(tr("重启当前设备的投屏服务。会先断开，再按当前配置重新连接。"));
    ui->startRecordBtn->setToolTip(m_recordingActive
        ? tr("当前状态：正在录制中。请使用“停止录制”按钮结束当前录制文件。")
        : tr("按全局配置的录制目录和格式开始录屏。不会重启当前投屏会话，每次开始都会生成一个新文件。"));
    ui->stopRecordBtn->setToolTip(m_recordingActive
        ? (m_recordingFilePath.isEmpty()
            ? tr("停止当前录屏并保存文件。")
            : tr("停止当前录屏并保存文件。\n当前文件：%1").arg(m_recordingFilePath))
        : tr("当前没有正在进行的录屏。"));
    ui->expandNotifyBtn->setToolTip(tr("展开当前设备的通知栏。"));
    ui->openScreenBtn->setToolTip(tr("点亮当前设备屏幕。"));
    ui->closeScreenBtn->setToolTip(tr("关闭当前设备屏幕显示，只熄屏，不会断开会话。"));
    ui->powerBtn->setToolTip(tr("向当前设备发送电源键。"));
    ui->volumeUpBtn->setToolTip(tr("向当前设备发送音量加键。"));
    ui->volumeDownBtn->setToolTip(tr("向当前设备发送音量减键。"));
    ui->appSwitchBtn->setToolTip(tr("打开当前设备的最近任务列表。"));
    ui->menuBtn->setToolTip(tr("向当前设备发送菜单键。"));
    ui->homeBtn->setToolTip(tr("返回当前设备主屏幕。"));
    ui->returnBtn->setToolTip(tr("向当前设备发送返回键。"));
    ui->screenShotBtn->setToolTip(tr("截取当前设备画面并按项目现有逻辑保存截图。"));
    ui->clipboardBtn->setToolTip(tr("读取当前设备剪贴板文本。"));
    ui->touchBtn->setToolTip(m_showTouch
        ? tr("当前状态：已开启触摸点显示。再次点击会关闭设备上的触摸点显示。")
        : tr("当前状态：未开启触摸点显示。点击后会在设备上显示触摸点。"));
    ui->groupControlBtn->setToolTip(m_isHost
        ? tr("当前状态：已作为群控主控设备。再次点击会取消主控状态。")
        : tr("当前状态：未作为群控主控设备。点击后会把当前设备设为群控主控。"));
}

void ToolForm::updateGroupControl()
{
    if (m_isHost) {
        ui->groupControlBtn->setStyleSheet("color: red");
    } else {
        ui->groupControlBtn->setStyleSheet("color: green");
    }

    updateToolTips();
    GroupController::instance().updateDeviceState(m_serial);
}

void ToolForm::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
#else
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
#endif
        event->accept();
    }
}

void ToolForm::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
}

void ToolForm::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        move(event->globalPos() - m_dragPosition);
#else
        move(event->globalPosition().toPoint() - m_dragPosition);
#endif
        event->accept();
    }
}

void ToolForm::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    qDebug() << "show event";
}

void ToolForm::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event)
    qDebug() << "hide event";
}

void ToolForm::on_fullScreenBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }

    dynamic_cast<VideoForm*>(parent())->switchFullScreen();
}

void ToolForm::on_returnBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->postGoBack();
}

void ToolForm::on_homeBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->postGoHome();
}

void ToolForm::on_menuBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->postGoMenu();
}

void ToolForm::on_appSwitchBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->postAppSwitch();
}

void ToolForm::on_powerBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->postPower();
}

void ToolForm::on_screenShotBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->screenshot();
}

void ToolForm::on_volumeUpBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->postVolumeUp();
}

void ToolForm::on_volumeDownBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->postVolumeDown();
}

void ToolForm::on_closeScreenBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->setDisplayPower(false);
}

void ToolForm::on_expandNotifyBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->expandNotificationPanel();
}

void ToolForm::on_touchBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }

    m_showTouch = !m_showTouch;
    device->showTouch(m_showTouch);
    updateToolTips();
}

void ToolForm::on_groupControlBtn_clicked()
{
    m_isHost = !m_isHost;
    updateGroupControl();
}

void ToolForm::on_openScreenBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->setDisplayPower(true);
}

void ToolForm::on_clipboardBtn_clicked()
{
    auto device = qsc::IDeviceManage::getInstance().getDevice(m_serial);
    if (!device) {
        return;
    }
    device->requestDeviceClipboard();
}

void ToolForm::on_restartServiceBtn_clicked()
{
    emit restartServiceRequested();
}

void ToolForm::on_startRecordBtn_clicked()
{
    emit startRecordingRequested();
}

void ToolForm::on_stopRecordBtn_clicked()
{
    emit stopRecordingRequested();
}
