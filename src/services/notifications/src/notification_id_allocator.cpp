// SPDX-License-Identifier: LGPL-3.0-or-later

#include "notification_id_allocator_p.h"

#include <limits>

namespace QindaQt::Services::Notifications::Private {

std::optional<quint32> NotificationIdAllocator::allocate(
    const IsUnavailable &isUnavailable)
{
    constexpr quint64 maximumId = std::numeric_limits<quint32>::max();
    quint64 candidate = m_nextGeneratedCandidate;
    while (candidate <= maximumId
           && isUnavailable(static_cast<quint32>(candidate))) {
        ++candidate;
    }
    if (candidate > maximumId) {
        return std::nullopt;
    }

    const auto generated = static_cast<quint32>(candidate);
    m_nextGeneratedCandidate = candidate + 1;
    return generated;
}

} // namespace QindaQt::Services::Notifications::Private
