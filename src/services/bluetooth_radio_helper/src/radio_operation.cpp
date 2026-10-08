// SPDX-License-Identifier: GPL-3.0-or-later
#include "radio_operation_p.h"
#include <utility>

namespace QindaQt::BluetoothRadio {
RadioOperation::RadioOperation(RadioAuthority &authority, RadioPlatform &platform,
    std::function<quint64()> clock)
    : m_authority(authority), m_platform(platform), m_clock(std::move(clock)) {}
bool RadioOperation::admitted(const QString &sender, const Request &request) {
    const auto now = m_clock ? m_clock() : 0;
    if (!now || request.deadlineBoottimeMs <= now
        || request.deadlineBoottimeMs - now > kRequestWindowMs
        || !m_authority.current(sender, request)) return false;
    const auto after = m_clock();
    return after && after >= now && after < request.deadlineBoottimeMs;
}
Result RadioOperation::execute(const QString &sender, const Request &request) {
    const auto reply = [&request](Disposition state, const char *reason) {
        return Result{request.nonce, state, QString::fromLatin1(reason)};
    };
    if (!validRequest(request))
        return reply(Disposition::Refused, "radio-request-rejected");
    if (!admitted(sender, request))
        return reply(Disposition::Refused, "radio-not-authorized");
    const auto now = m_clock();
    for (auto it = m_seen.begin(); it != m_seen.end();) {
        if (it.value() <= now) it = m_seen.erase(it);
        else ++it;
    }
    const auto key = qMakePair(sender, request.nonce);
    if (m_seen.contains(key))
        return reply(Disposition::Refused, "radio-request-rejected");
    if (m_seen.size() >= kMaxLiveRequests)
        return reply(Disposition::Refused, "radio-busy");
    // Record before platform calls and retain across A/B/A alias transitions.
    // A global bound refuses new work rather than evicting unexpired authority.
    // A repeated owner/nonce never repeats a write or returns stale observation.
    m_seen.insert(key, request.deadlineBoottimeMs);
    auto selection = m_platform.select(request.adapterPath);
    if (!admitted(sender, request))
        return reply(Disposition::Refused, "radio-not-authorized");
    if (!selection.lease) {
        if (selection.reasonCode == QLatin1String("radio-observation-unavailable"))
            return reply(Disposition::NoWriteUnavailable, "radio-observation-unavailable");
        return reply(Disposition::Refused, "radio-stale-target");
    }
    auto observed = selection.lease->observe();
    if (!observed.current) return reply(Disposition::Refused, "radio-stale-target");
    if (!admitted(sender, request))
        return reply(Disposition::Refused, "radio-not-authorized");
    observed = selection.lease->observe();
    if (!observed.current) return reply(Disposition::Refused, "radio-stale-target");
    if (observed.hardBlocked)
        return reply(Disposition::Refused, "radio-hardware-blocked");
    if (!observed.softBlocked)
        return reply(Disposition::VerifiedUnblocked, "radio-unblocked");
    const auto written = selection.lease->unblock([&] { return admitted(sender, request); });
    if (written != RadioWrite::Attempted) {
        return reply(Disposition::Refused, written == RadioWrite::Denied
            ? "radio-not-authorized" : "radio-software-blocked");
    }
    // The syscall may have affected the radio even on a short/error result.
    // There is no rollback, second write, or retry after any uncertainty.
    observed = selection.lease->observe();
    if (!admitted(sender, request) || !observed.current)
        return reply(Disposition::Uncertain, "radio-change-uncertain");
    observed = selection.lease->observe();
    if (!observed.current || observed.softBlocked || observed.hardBlocked)
        return reply(Disposition::Uncertain, "radio-change-uncertain");
    return reply(Disposition::VerifiedUnblocked, "radio-unblocked");
}
} // namespace QindaQt::BluetoothRadio
