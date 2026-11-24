#include <QDebug>

#include "bufferutil.h"
#include "devicemsg.h"

DeviceMsg::DeviceMsg(QObject *parent) : QObject(parent) {}

DeviceMsg::~DeviceMsg()
{
    if (DMT_GET_CLIPBOARD == m_data.type && Q_NULLPTR != m_data.clipboardMsg.text) {
        delete[] m_data.clipboardMsg.text;
        m_data.clipboardMsg.text = Q_NULLPTR;
    }
}

DeviceMsg::DeviceMsgType DeviceMsg::type()
{
    return m_data.type;
}

void DeviceMsg::getClipboardMsgData(QString &text)
{
    if (m_data.clipboardMsg.text) {
        text = QString::fromUtf8(m_data.clipboardMsg.text);
    } else {
        text.clear();
    }
}

qint32 DeviceMsg::deserialize(QByteArray &byteArray)
{
    QBuffer buf(&byteArray);
    buf.open(QBuffer::ReadOnly);

    const qint64 len = buf.size();
    char c = 0;
    qint32 ret = 0;

    if (len < 1) {
        // at least the message type
        return 0; // not available
    }

    buf.getChar(&c);
    m_data.type = static_cast<DeviceMsgType>(static_cast<unsigned char>(c));
    switch (m_data.type) {
    case DMT_GET_CLIPBOARD: {
        m_data.clipboardMsg.text = Q_NULLPTR;
        if (len < 5) {
            ret = 0; // not available
            break;
        }

        const quint32 clipboardLen = BufferUtil::read32(buf);
        if (clipboardLen > DEVICE_MSG_TEXT_MAX_LENGTH) {
            qWarning("Clipboard text exceeds maximum length: %u", clipboardLen);
            ret = -1;
            break;
        }
        if (clipboardLen > static_cast<quint64>(len - 5)) {
            ret = 0; // not available
            break;
        }

        const QByteArray text = buf.read(clipboardLen);
        if (static_cast<quint32>(text.length()) < clipboardLen) {
            ret = 0; // not available
            break;
        }

        m_data.clipboardMsg.text = new char[text.length() + 1];
        memcpy(m_data.clipboardMsg.text, text.data(), static_cast<size_t>(text.length()));
        m_data.clipboardMsg.text[text.length()] = '\0';

        ret = static_cast<qint32>(5 + clipboardLen);
        break;
    }
    case DMT_ACK_CLIPBOARD:
        if (len < 9) {
            ret = 0; // not available
            break;
        }
        ret = 9;
        break;
    case DMT_UHID_OUTPUT: {
        if (len < 5) {
            ret = 0; // not available
            break;
        }

        BufferUtil::read16(buf); // id
        const quint16 dataLen = BufferUtil::read16(buf);
        if (static_cast<qint64>(dataLen) > len - 5) {
            ret = 0; // not available
            break;
        }
        ret = static_cast<qint32>(5 + dataLen);
        break;
    }
    default:
        qWarning("Unsupported device msg type: %d", static_cast<int>(m_data.type));
        ret = -1; // error, the protocol does not expose a generic frame size
        break;
    }

    buf.close();
    return ret;
}
