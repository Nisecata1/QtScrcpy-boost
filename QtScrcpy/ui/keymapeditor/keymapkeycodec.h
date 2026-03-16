#ifndef KEYMAPKEYCODEC_H
#define KEYMAPKEYCODEC_H

#include <QMetaEnum>
#include <QString>
#include <Qt>

#include "../../QtScrcpyCore/src/device/controller/inputconvert/keymap/keymap.h"

class KeymapKeyCodec
{
public:
    static bool decodeJsonKey(const QString &jsonKey, KeyMap::ActionType &typeOut, int &valueOut);
    static QString encodeKey(int qtKey);
    static QString encodeMouseButton(int mouseButton);
    static QString encodeAction(KeyMap::ActionType type, int value);
    static QString displayStringForAction(KeyMap::ActionType type, int value);
    static QString displayStringForJsonKey(const QString &jsonKey);

private:
    static QMetaEnum keyEnum();
    static QMetaEnum mouseEnum();
};

#endif // KEYMAPKEYCODEC_H
