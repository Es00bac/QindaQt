// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QQmlComponent;
class QString;

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: Call on the component/engine GUI thread with a live Qt event
// dispatcher. This may run a nested event loop for at most five seconds while
// local imports resolve; object construction and ownership remain with the
// caller, and every failure returns a bounded non-empty diagnostic.
[[nodiscard]] bool awaitQmlComponentReady(QQmlComponent &component,
                                          QString *diagnostic);

} // namespace QindaQt::Apps::FileManager
