// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::SessionSupervisor {

// Well-known distribution locations of the polkit KDE authentication agent.
// The first existing executable wins when the operator neither configures a
// path nor disables the optional agent.
[[nodiscard]] QStringList defaultPolkitAgentCandidates();

// Selection truth for the supervisor's optional polkit agent.
//
// AGENT-CONTRACT: `disabled` wins over every configured path so private and
// integration runs can prove they never launch a host agent. An omitted
// configuration keeps the installed production behavior: the first existing
// well-known candidate, or empty when the host offers none. An explicit
// configuration is passed through unchanged; the supervisor's start path
// independently skips non-executable programs.
[[nodiscard]] QString resolvePolkitAgentExecutable(bool disabled,
                                                   const QString &configured);

} // namespace QindaQt::SessionSupervisor
