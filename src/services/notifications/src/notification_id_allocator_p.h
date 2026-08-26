// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtTypes>

#include <functional>
#include <optional>

namespace QindaQt::Services::Notifications::Private {

class NotificationIdAllocator final {
public:
    using IsUnavailable = std::function<bool(quint32)>;

    // Generated IDs are monotonically increasing and therefore are never
    // reused during this service lifetime. The predicate represents only
    // currently active client-supplied replaces_id values that the next
    // generated ID must skip.
    [[nodiscard]] std::optional<quint32> allocate(
        const IsUnavailable &isUnavailable);

private:
    // AGENT-CONTRACT: Explicit replaces_id values are caller-supplied, not
    // server-generated IDs. Once closed, the notification specification says
    // they are invalidated. Retaining their sparse history forever would let
    // one client consume a global lifetime data structure and deny unrelated
    // clients, so only the monotonic generated sequence is retained here.
    quint64 m_nextGeneratedCandidate = 1;
};

} // namespace QindaQt::Services::Notifications::Private
