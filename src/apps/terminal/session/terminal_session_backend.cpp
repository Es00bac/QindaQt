// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/terminal_session_backend.h"

namespace QindaQt::Apps::Terminal {

TerminalSessionBackend::TerminalSessionBackend(QObject *parent)
    : QObject(parent) {}

TerminalSessionBackend::~TerminalSessionBackend() = default;

} // namespace QindaQt::Apps::Terminal
