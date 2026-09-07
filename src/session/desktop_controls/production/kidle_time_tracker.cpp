// SPDX-License-Identifier: LGPL-3.0-or-later
#include "kidle_time_tracker.h"

#include <KIdleTime>

namespace QindaQt::Session::DesktopControls {

KIdleTimeTracker::KIdleTimeTracker(QObject *parent)
    : IdleTracker(parent)
{
    auto *const idleTime = KIdleTime::instance();
    connect(idleTime, &KIdleTime::timeoutReached, this,
            [this](int identifier, int msec) {
                const auto entry = m_tokenForMilliseconds.constFind(msec);
                if (entry == m_tokenForMilliseconds.constEnd()
                    || entry.value() != identifier) {
                    return;
                }
                Q_EMIT timeoutReached(msec);
            });
    connect(idleTime, &KIdleTime::resumingFromIdle, this, [this] {
        // AGENT-CONTRACT: KIdleTime delivers resumingFromIdle only once per
        // armed catch; re-arm immediately so the next idle period reports
        // again. See KIdleTime::catchNextResumeEvent documentation.
        KIdleTime::instance()->catchNextResumeEvent();
        Q_EMIT resumingFromIdle();
    });
    idleTime->catchNextResumeEvent();
}

KIdleTimeTracker::~KIdleTimeTracker() = default;

void KIdleTimeTracker::armTimeout(int milliseconds)
{
    if (milliseconds <= 0 || m_tokenForMilliseconds.contains(milliseconds)) {
        return;
    }
    const int token = KIdleTime::instance()->addIdleTimeout(milliseconds);
    m_tokenForMilliseconds.insert(milliseconds, token);
}

void KIdleTimeTracker::disarmTimeout(int milliseconds)
{
    const auto token = m_tokenForMilliseconds.find(milliseconds);
    if (token == m_tokenForMilliseconds.end()) {
        return;
    }
    KIdleTime::instance()->removeIdleTimeout(token.value());
    m_tokenForMilliseconds.erase(token);
}

} // namespace QindaQt::Session::DesktopControls
