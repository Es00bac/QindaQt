// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/app_shell/app_shell_types.h"

namespace QindaQt::AppShell {

Error makeError(ErrorCode code, const QString &message, bool recoverable)
{
    Error error;
    error.code = code;
    error.message = message.left(MaximumDiagnosticLength);
    error.recoverable = recoverable;
    return error;
}

} // namespace QindaQt::AppShell
