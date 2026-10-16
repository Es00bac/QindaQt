// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/qtypes.h>

namespace QindaQt::Audio
{

// AGENT-CONTRACT (ADR-0394): the delay schedule for a client that retries
// after the service rejected, lost or contradicted its snapshot. The first
// retry waits `initialMs`, each following retry doubles the wait, and no wait
// exceeds `maximumMs`; a healthy answer calls reset() so the next failure
// starts from the bottom again. A flat 200 ms retry against a service that is
// down or that keeps answering with a contradiction costs five wakeups a
// second per client for as long as the condition lasts (the OBS bridge kept
// that up for the life of the OBS process).
//
// Deliberately deterministic, with no jitter and no clock: the owner turns
// each answer into a timer interval, and tests read the schedule directly.
class RetryBackoff
{
public:
    constexpr explicit RetryBackoff(int initialMs = 200, int maximumMs = 10'000) noexcept
        : m_initial(initialMs > 0 ? initialMs : 1)
        , m_maximum(maximumMs >= m_initial ? maximumMs : m_initial)
        , m_next(m_initial)
    {
    }

    // The wait for the retry being scheduled now. The following call answers
    // double this, never above the maximum.
    [[nodiscard]] constexpr int take() noexcept
    {
        const int delay = m_next;
        m_next = m_next > m_maximum / 2 ? m_maximum : m_next * 2;
        return delay;
    }

    // What take() would answer, without advancing the schedule.
    [[nodiscard]] constexpr int peek() const noexcept { return m_next; }

    // A healthy answer arrived: the next failure retries after `initialMs`.
    constexpr void reset() noexcept { m_next = m_initial; }

    [[nodiscard]] constexpr int initial() const noexcept { return m_initial; }
    [[nodiscard]] constexpr int maximum() const noexcept { return m_maximum; }

private:
    int m_initial;
    int m_maximum;
    int m_next;
};

} // namespace QindaQt::Audio
