// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/shell/global_menu/ownership/window_identity.h>

#include <QtCore/QtGlobal>

#include <optional>

namespace QindaQt::Shell::GlobalMenu::Composition
{

// Injected join from the compositor-authenticated opaque window identity to
// the legacy registrar's uint32 window id. A missing or ambiguous mapping
// means no legacy menu; client-supplied ids never create this join.
class RegistrarWindowIdSource
{
public:
    virtual ~RegistrarWindowIdSource() = default;
    [[nodiscard]] virtual std::optional<quint32> registrarWindowIdFor(
        const Ownership::WindowIdentity &window) const = 0;
};

} // namespace QindaQt::Shell::GlobalMenu::Composition
