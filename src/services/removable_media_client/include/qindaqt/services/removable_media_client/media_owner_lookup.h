// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDBusPendingCall>

namespace QindaQt::RemovableMedia {
// Observational asynchronous owner-discovery port. It queries the fixed media
// service without activation; no command/name supplied by a consumer. Calls
// occur on the client thread. Injected instances outlive the client and return
// owning pending-call values; transient failures never imply launch/readiness.
class MediaOwnerLookup {
public:
    virtual ~MediaOwnerLookup() = default;
    [[nodiscard]] virtual QDBusPendingCall query() = 0;
};
}
