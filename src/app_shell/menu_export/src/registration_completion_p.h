// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

class QDBusMessage;

namespace QindaQt::AppShell::MenuExport {

enum class RegistrationCompletion { Confirmed, Refused, Uncertain };

[[nodiscard]] RegistrationCompletion
classifyRegistrationCompletion(const QDBusMessage &reply);

} // namespace QindaQt::AppShell::MenuExport
