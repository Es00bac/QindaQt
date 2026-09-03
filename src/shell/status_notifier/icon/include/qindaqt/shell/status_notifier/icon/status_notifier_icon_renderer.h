// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_types.h>

#include <QtGui/QImage>

#include <QtCore/QStringList>

#include <memory>

namespace QindaQt::StatusNotifier
{

class StatusNotifierIconLocator;

// AGENT-CONTRACT: Icon renderer for tray presentation. Resolution order for
// render(icon, size) is:
// 1. The nearest bounded pixmap from the wire payload, decoded as
//    premultiplied ARGB32 (exact width * height * 4 accounting; a pixmap that
//    fails the foundation bounds decodes to null and is skipped).
// 2. Icon-theme lookup by IconName through the injected locator.
// 3. A deterministic neutral placeholder, so a missing or hostile icon can
//    never produce a null or randomly sized image in presentation.
// Theme image metadata is dimension-checked before decode, decoded dimensions
// are checked again, and placeholder requests are clamped to the same shared
// 512-pixel ceiling as wire pixmaps.
//
// No network access, no filesystem writes, and no reads outside the injected
// theme roots. Threading: pure lookup plus QImage value construction; an
// instance is safe to use from the thread that owns it.
class StatusNotifierIconRenderer
{
public:
    explicit StatusNotifierIconRenderer(QStringList themeRoots);
    ~StatusNotifierIconRenderer();
    StatusNotifierIconRenderer(const StatusNotifierIconRenderer &) = delete;
    StatusNotifierIconRenderer &operator=(const StatusNotifierIconRenderer &) = delete;
    StatusNotifierIconRenderer(StatusNotifierIconRenderer &&) = delete;
    StatusNotifierIconRenderer &operator=(StatusNotifierIconRenderer &&) = delete;

    [[nodiscard]] QImage render(const IconPayload &icon, int size) const;

    // Decodes one wire pixmap into an ARGB32 premultiplied QImage; null when
    // the byte accounting or dimension bounds are violated.
    [[nodiscard]] static QImage decodePixmap(const Pixmap &pixmap);

    // Deterministic placeholder for a refused, missing, or undecodable icon.
    [[nodiscard]] static QImage fallbackIcon(int size);

private:
    [[nodiscard]] QImage decodeThemeIcon(const QString &iconName, int size) const;

    std::unique_ptr<StatusNotifierIconLocator> m_locator;
};

} // namespace QindaQt::StatusNotifier
