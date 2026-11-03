// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_focus/focus_source.h>

#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>

#include <functional>

namespace QindaQt::Link {

// The focus fact most recently published through org.qindaqt.Link1.Focus.Publish.
// The D-Bus adaptor authorizes the shell; this class only validates and stores.
//
// Wire form (ADR-0440): `{"uuid":s,"appId":s,"title":s,"revision":n}`, or
// `{}` / `{"none":true}` when the facts are unavailable.
// Bounds: at most kMaxJsonBytes of JSON; uuid 1-64 and appId 0-128 printable
// ASCII characters without whitespace; revision a non-negative integer below 2^53;
// title any text, which is sanitized (controls and bidi overrides become spaces)
// and cut to kMaxTitleChars code points.
// Every accepted publish (even an identical one) refreshes the fact's age; see kStaleMs.
// Error behavior: publish() returns Rejected and leaves the previous fact untouched.
class FocusStore final : public FocusSource {
    Q_OBJECT
public:
    static constexpr int kMaxJsonBytes = 2048;
    static constexpr int kMaxTitleChars = 200;
    // A fact older than this is treated as unknown: the shell republishes every second, so silence
    // means it hung or died with the bus name still held (ADR-0440).
    static constexpr qint64 kStaleMs = 3000;
    enum class Result { Accepted, Cleared, Rejected };

    using FocusSource::FocusSource;
    [[nodiscard]] Result publish(const QByteArray &json);
    // Forgets the fact (the publisher went away).
    void clear();
    [[nodiscard]] std::optional<FocusFact> current() const override;
    // Test seam: milliseconds on a monotonic clock. Default is a running QElapsedTimer.
    void setClock(std::function<qint64()> clock) { m_clock = std::move(clock); }

private:
    [[nodiscard]] qint64 now() const { return m_clock ? m_clock() : m_timer.elapsed(); }

    std::optional<FocusFact> m_fact;
    qint64 m_publishedAt = 0;
    std::function<qint64()> m_clock;
    QElapsedTimer m_timer = [] { QElapsedTimer t; t.start(); return t; }();
};

} // namespace QindaQt::Link
