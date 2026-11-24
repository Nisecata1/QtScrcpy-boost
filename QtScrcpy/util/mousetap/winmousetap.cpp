#include <QDebug>
#include <QWidget>
#include <Windows.h>

#include "winmousetap.h"

WinMouseTap::WinMouseTap() {}

WinMouseTap::~WinMouseTap() {}

void WinMouseTap::initMouseEventTap() {}

void WinMouseTap::quitMouseEventTap() {}

void WinMouseTap::enableMouseEventTap(QRect rc, bool enabled)
{
    if (enabled && rc.isEmpty()) {
        return;
    }
    if (enabled) {
        RECT mainRect;
        mainRect.left = static_cast<LONG>(rc.left());
        mainRect.top = static_cast<LONG>(rc.top());
        // Win32 ClipCursor uses exclusive right/bottom bounds.
        mainRect.right = static_cast<LONG>(rc.left() + rc.width());
        mainRect.bottom = static_cast<LONG>(rc.top() + rc.height());
        ClipCursor(&mainRect);
    } else {
        ClipCursor(Q_NULLPTR);
    }
}
