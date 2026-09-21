// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/voice_protocol/voice_types.h>

namespace QindaQt::Services::Voice {

bool capabilityForKind(const OperationKind kind, const quint32 capabilities) noexcept
{
    // AGENT-NOTE: starting, finishing, cancelling and arming are the contract
    // floor. A provider that answers Voice1 at all can do them; everything else
    // is optional and must be advertised before the shell offers it.
    switch (kind) {
    case OperationKind::StartDictation:
    case OperationKind::Finish:
    case OperationKind::Cancel:
    case OperationKind::SetEnabled:
        return true;
    case OperationKind::StartCommand:
        return (capabilities & CapabilityCommandMode) != 0;
    case OperationKind::Retry:
        return (capabilities & CapabilityRetry) != 0;
    case OperationKind::Undo:
        return (capabilities & CapabilityUndo) != 0;
    case OperationKind::CopyLast:
        return (capabilities & CapabilityCopy) != 0;
    case OperationKind::SetProvider:
        return (capabilities & CapabilityProviderSelection) != 0;
    }
    return false;
}

} // namespace QindaQt::Services::Voice
