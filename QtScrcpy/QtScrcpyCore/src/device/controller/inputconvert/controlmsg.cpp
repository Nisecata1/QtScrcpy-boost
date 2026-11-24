#include <cstring>

#include <QDebug>

#include "bufferutil.h"
#include "controlmsg.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define CLAMP(V, X, Y) MIN(MAX((V), (X)), (Y))

namespace {
QByteArray truncateUtf8Bytes(const QString &text, int maxBytes)
{
    QByteArray utf8 = text.toUtf8();
    if (utf8.size() <= maxBytes) {
        return utf8;
    }
    int end = qMax(0, maxBytes);
    while (end > 0 && (static_cast<unsigned char>(utf8.at(end)) & 0xC0) == 0x80) {
        --end;
    }
    utf8.truncate(end);
    return utf8;
}
}

ControlMsg::ControlMsg(ControlMsgType controlMsgType) : QScrcpyEvent(Control)
{
    std::memset(&m_data, 0, sizeof(m_data));
    m_data.type = controlMsgType;
}

ControlMsg::~ControlMsg()
{
    if (CMT_SET_CLIPBOARD == m_data.type && Q_NULLPTR != m_data.setClipboard.text) {
        delete [] m_data.setClipboard.text;
        m_data.setClipboard.text = Q_NULLPTR;
    } else if (CMT_INJECT_TEXT == m_data.type && Q_NULLPTR != m_data.injectText.text) {
        delete [] m_data.injectText.text;
        m_data.injectText.text = Q_NULLPTR;
    }
}

void ControlMsg::setInjectKeycodeMsgData(AndroidKeyeventAction action, AndroidKeycode keycode, quint32 repeat, AndroidMetastate metastate)
{
    m_data.injectKeycode.action = action;
    m_data.injectKeycode.keycode = keycode;
    m_data.injectKeycode.repeat = repeat;
    m_data.injectKeycode.metastate = metastate;
}

void ControlMsg::setInjectTextMsgData(QString &text)
{
    delete [] m_data.injectText.text;
    m_data.injectText.text = Q_NULLPTR;

    const QByteArray tmp = truncateUtf8Bytes(text, CONTROL_MSG_INJECT_TEXT_MAX_LENGTH);
    if (tmp.size() != text.toUtf8().size()) {
        text = QString::fromUtf8(tmp);
    }
    m_data.injectText.text = new char[static_cast<size_t>(tmp.length()) + 1];
    memcpy(m_data.injectText.text, tmp.data(), static_cast<size_t>(tmp.length()));
    m_data.injectText.text[tmp.length()] = '\0';
}

void ControlMsg::setInjectTouchMsgData(
    quint64 id,
    AndroidMotioneventAction action,
    AndroidMotioneventButtons actionButtons,
    AndroidMotioneventButtons buttons,
    QRect position,
    float pressure)
{
    m_data.injectTouch.id = id;
    m_data.injectTouch.action = action;
    m_data.injectTouch.actionButtons = actionButtons;
    m_data.injectTouch.buttons = buttons;
    m_data.injectTouch.position = position;
    m_data.injectTouch.pressure = pressure;
}

void ControlMsg::setInjectScrollMsgData(QRect position, float hScroll, float vScroll, AndroidMotioneventButtons buttons)
{
    m_data.injectScroll.position = position;
    m_data.injectScroll.hScroll = hScroll;
    m_data.injectScroll.vScroll = vScroll;
    m_data.injectScroll.buttons = buttons;
}

void ControlMsg::setGetClipboardMsgData(ControlMsg::GetClipboardCopyKey copyKey) 
{
    m_data.getClipboard.copyKey = copyKey;
}

void ControlMsg::setSetClipboardMsgData(QString &text, bool paste)
{
    delete [] m_data.setClipboard.text;
    m_data.setClipboard.text = Q_NULLPTR;
    m_data.setClipboard.paste = paste;
    m_data.setClipboard.sequence = 0;

    if (text.isEmpty()) {
        return;
    }

    const QByteArray tmp = truncateUtf8Bytes(text, CONTROL_MSG_CLIPBOARD_TEXT_MAX_LENGTH);
    if (tmp.size() != text.toUtf8().size()) {
        text = QString::fromUtf8(tmp);
    }
    m_data.setClipboard.text = new char[static_cast<size_t>(tmp.length()) + 1];
    memcpy(m_data.setClipboard.text, tmp.data(), static_cast<size_t>(tmp.length()));
    m_data.setClipboard.text[tmp.length()] = '\0';
}

void ControlMsg::setDisplayPowerData(bool on)
{
    m_data.setDisplayPower.on = on;
}

void ControlMsg::setBackOrScreenOnData(bool down)
{
    m_data.backOrScreenOn.action = down ? AKEY_EVENT_ACTION_DOWN : AKEY_EVENT_ACTION_UP;
}

void ControlMsg::setInjectCursorMsgData(qint32 x, qint32 y, qint32 w, qint32 h)
{
    m_data.injectCursor.x = x;
    m_data.injectCursor.y = y;
    m_data.injectCursor.w = w;
    m_data.injectCursor.h = h;
}

void ControlMsg::setCursorConfigMsgData(qint32 cursorSizePx)
{
    m_data.cursorConfig.cursorSizePx = cursorSizePx;
}

void ControlMsg::writePosition(QBuffer &buffer, const QRect &value)
{
    BufferUtil::write32(buffer, value.left());
    BufferUtil::write32(buffer, value.top());
    BufferUtil::write16(buffer, value.width());
    BufferUtil::write16(buffer, value.height());
}

quint16 ControlMsg::flostToU16fp(float f)
{
    Q_ASSERT(f >= 0.0f && f <= 1.0f);
    quint32 u = f * 0x1p16f; // 2^16
    if (u >= 0xffff) {
        u = 0xffff;
    }
    return (quint16)u;
}

qint16 ControlMsg::flostToI16fp(float f)
{
    Q_ASSERT(f >= -1.0f && f <= 1.0f);
    qint32 i = f * 0x1p15f; // 2^15
    Q_ASSERT(i >= -0x8000);
    if (i >= 0x7fff) {
        Q_ASSERT(i == 0x8000); // for f == 1.0f
        i = 0x7fff;
    }
    return (qint16)i;
}

QByteArray ControlMsg::serializeData()
{
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QBuffer::WriteOnly);
    buffer.putChar(m_data.type);

    switch (m_data.type) {
    case CMT_INJECT_KEYCODE:
        buffer.putChar(m_data.injectKeycode.action);
        BufferUtil::write32(buffer, m_data.injectKeycode.keycode);
        BufferUtil::write32(buffer, m_data.injectKeycode.repeat);
        BufferUtil::write32(buffer, m_data.injectKeycode.metastate);
        break;
    case CMT_INJECT_TEXT:
        if (m_data.injectText.text != Q_NULLPTR) {
            const size_t len = strlen(m_data.injectText.text);
            BufferUtil::write32(buffer, static_cast<quint32>(len));
            buffer.write(m_data.injectText.text, static_cast<qint64>(len));
        } else {
            BufferUtil::write32(buffer, 0);
        }
        break;
    case CMT_INJECT_TOUCH: {
        buffer.putChar(m_data.injectTouch.action);
        BufferUtil::write64(buffer, m_data.injectTouch.id);
        writePosition(buffer, m_data.injectTouch.position);
        quint16 pressure = flostToU16fp(m_data.injectTouch.pressure);
        BufferUtil::write16(buffer, pressure);
        BufferUtil::write32(buffer, m_data.injectTouch.actionButtons);
        BufferUtil::write32(buffer, m_data.injectTouch.buttons);
    } break;
    case CMT_INJECT_SCROLL: {
        writePosition(buffer, m_data.injectScroll.position);
        // Accept values in the range [-16, 16].
        // Normalize to [-1, 1] in order to use sc_float_to_i16fp().
        float hscrollNorm = m_data.injectScroll.hScroll / 16;
        hscrollNorm = CLAMP(hscrollNorm, -1, 1);
        float vscrollNorm = m_data.injectScroll.vScroll / 16;
        vscrollNorm = CLAMP(vscrollNorm, -1, 1);
        qint16 hScroll = flostToI16fp(hscrollNorm);
        qint16 vScroll = flostToI16fp(vscrollNorm);
        BufferUtil::write16(buffer, (quint16)hScroll);
        BufferUtil::write16(buffer, (quint16)vScroll);
        BufferUtil::write32(buffer, m_data.injectScroll.buttons);
    } break;
    case CMT_BACK_OR_SCREEN_ON:
        buffer.putChar(m_data.backOrScreenOn.action);
        break;
    case CMT_GET_CLIPBOARD:
        buffer.putChar(m_data.getClipboard.copyKey);
        break;
    case CMT_SET_CLIPBOARD:
        BufferUtil::write64(buffer, m_data.setClipboard.sequence);
        buffer.putChar(!!m_data.setClipboard.paste);
        if (m_data.setClipboard.text != Q_NULLPTR) {
            const size_t len = strlen(m_data.setClipboard.text);
            BufferUtil::write32(buffer, static_cast<quint32>(len));
            buffer.write(m_data.setClipboard.text, static_cast<qint64>(len));
        } else {
            BufferUtil::write32(buffer, 0);
        }
        break;
    case CMT_SET_DISPLAY_POWER:
        buffer.putChar(m_data.setDisplayPower.on);
        break;
    case CMT_INJECT_CURSOR:
        BufferUtil::write32(buffer, static_cast<quint32>(m_data.injectCursor.x));
        BufferUtil::write32(buffer, static_cast<quint32>(m_data.injectCursor.y));
        BufferUtil::write32(buffer, static_cast<quint32>(m_data.injectCursor.w));
        BufferUtil::write32(buffer, static_cast<quint32>(m_data.injectCursor.h));
        break;
    case CMT_SET_CURSOR_CONFIG:
        BufferUtil::write32(buffer, static_cast<quint32>(m_data.cursorConfig.cursorSizePx));
        break;
    case CMT_EXPAND_NOTIFICATION_PANEL:
    case CMT_EXPAND_SETTINGS_PANEL:
    case CMT_COLLAPSE_PANELS:
    case CMT_ROTATE_DEVICE:
        break;
    default:
        qDebug() << "Unknown event type:" << m_data.type;
        break;
    }
    buffer.close();
    return byteArray;
}
