// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/window_identity.h>

#include <QtCore/QString>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Composition
{

struct AnnouncedMenuAddress final {
    QString serviceName;
    QString objectPath;

    bool operator==(const AnnouncedMenuAddress &) const = default;
};

// Authenticated compositor projection used only when a native Wayland window
// has no numeric AppMenu registrar id. The address is an announcement, not an
// ownership proof: the coordinator resolves its current unique bus owner and
// runs the same PID/focus authentication used for registrar entries.
class AnnouncedMenuAddressSource
{
public:
    virtual ~AnnouncedMenuAddressSource() = default;
    [[nodiscard]] virtual std::optional<AnnouncedMenuAddress> announcedMenuFor(
        const Ownership::WindowIdentity &window) const = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Composition
