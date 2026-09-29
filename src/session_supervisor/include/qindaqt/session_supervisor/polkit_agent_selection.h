// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::SessionSupervisor {

// ADR-0290: QindaQt runs only its own native polkit authentication agent,
// with no KDE (or other distribution) fallback. The single candidate is the
// agent's fixed install path, built from KDE_INSTALL_FULL_LIBEXECDIR by
// session_supervisor's CMakeLists.txt (QINDAQT_POLKIT_AGENT_INSTALL_PATH).
//
// AGENT-CONTRACT: the literal executable basename "qindaqt-polkit-agent"
// here and in src/apps/polkit_agent/CMakeLists.txt's install() rule must
// stay identical; nothing in CMake ties the two together.
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
