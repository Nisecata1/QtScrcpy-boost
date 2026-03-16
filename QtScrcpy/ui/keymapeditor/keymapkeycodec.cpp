#include "keymapkeycodec.h"

#include <QKeySequence>

QMetaEnum KeymapKeyCodec::keyEnum()
{
    return QMetaEnum::fromType<Qt::Key>();
}

QMetaEnum KeymapKeyCodec::mouseEnum()
{
    return QMetaEnum::fromType<Qt::MouseButtons>();
}

bool KeymapKeyCodec::decodeJsonKey(const QString &jsonKey, KeyMap::ActionType &typeOut, int &valueOut)
{
    const QByteArray encoded = jsonKey.toUtf8();
    const int key = keyEnum().keyToValue(encoded.constData());
    if (key != -1) {
        typeOut = KeyMap::AT_KEY;
        valueOut = key;
        return true;
    }

    const int mouse = mouseEnum().keyToValue(encoded.constData());
    if (mouse != -1) {
        typeOut = KeyMap::AT_MOUSE;
        valueOut = mouse;
        return true;
    }

    typeOut = KeyMap::AT_INVALID;
    valueOut = Qt::Key_unknown;
    return false;
}

QString KeymapKeyCodec::encodeKey(int qtKey)
{
    const char *name = keyEnum().valueToKey(qtKey);
    return name ? QString::fromLatin1(name) : QString();
}

QString KeymapKeyCodec::encodeMouseButton(int mouseButton)
{
    const char *name = mouseEnum().valueToKey(mouseButton);
    return name ? QString::fromLatin1(name) : QString();
}

QString KeymapKeyCodec::encodeAction(KeyMap::ActionType type, int value)
{
    switch (type) {
    case KeyMap::AT_KEY:
        return encodeKey(value);
    case KeyMap::AT_MOUSE:
        return encodeMouseButton(value);
    default:
        return QString();
    }
}

QString KeymapKeyCodec::displayStringForAction(KeyMap::ActionType type, int value)
{
    if (type == KeyMap::AT_KEY) {
        const QString native = QKeySequence(value).toString(QKeySequence::NativeText);
        if (!native.isEmpty()) {
            return native;
        }
    }
    return encodeAction(type, value);
}

QString KeymapKeyCodec::displayStringForJsonKey(const QString &jsonKey)
{
    KeyMap::ActionType type = KeyMap::AT_INVALID;
    int value = Qt::Key_unknown;
    if (!decodeJsonKey(jsonKey, type, value)) {
        return jsonKey;
    }
    return displayStringForAction(type, value);
}
