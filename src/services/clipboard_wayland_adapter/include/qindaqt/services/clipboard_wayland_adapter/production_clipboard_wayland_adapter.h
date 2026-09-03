// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_wayland_adapter/clipboard_wayland_adapter.h>

#include <memory>

namespace QindaQt::Services::ClipboardWayland {

[[nodiscard]] std::unique_ptr<ClipboardWaylandAdapter>
makeProductionClipboardWaylandAdapter();

} // namespace QindaQt::Services::ClipboardWayland
