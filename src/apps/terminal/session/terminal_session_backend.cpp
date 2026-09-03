// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_session_backend.h"

namespace QindaQt::Apps::Terminal {

TerminalSessionBackend::TerminalSessionBackend(QObject *parent)
    : QObject(parent) {}

TerminalSessionBackend::~TerminalSessionBackend() = default;

TerminalSearchResult TerminalSessionBackend::searchScrollback(
    const TerminalSearchQuery &, TerminalSearchDirection) {
  return {.diagnostic = QStringLiteral("Scrollback search is unavailable")};
}

void TerminalSessionBackend::clearScrollbackSearch() {}

TerminalLinkSelection TerminalSessionBackend::selectVisibleLink(int) {
  return {};
}

TerminalLinkSelection TerminalSessionBackend::currentVisibleLink() {
  return {};
}

} // namespace QindaQt::Apps::Terminal
