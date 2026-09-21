// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

namespace QindaQt::Compositor::KWinIntegration {

// The upper-left screen corner, for a POINTER rather than a finger.
//
// AGENT-CONTRACT (ADR-0232): SessionDefaults seeds an empty
// Effect-overview/BorderActivate so KWin's own window grid no longer answers
// that corner. Un-reserving it is only half the promise - something of ours
// has to claim it, and the touch-edge machinery next door cannot: KWin's
// reserveTouch is for touch only, and a corner is not one of its four edges.
//
// This is deliberately narrow. One corner, one action, no settings key: the
// gather overview is what the upper-left corner means in QindaQt, and a user
// who wants it back on KWin's grid says so in kwinrc, which SessionDefaults
// then leaves alone. Widen it to the other three corners only when there is
// something to put in them.
class PointerCornerReserver {
public:
    virtual ~PointerCornerReserver();
    // Callbacks are invoked by name through QMetaObject::invokeMethod, which
    // is KWin's own contract for an externally reserved edge.
    virtual void reserve(QObject *object, const char *callback) = 0;
    virtual void unreserve(QObject *object) = 0;
};

// Owns the reservation and turns a corner trigger into the same
// (edge, action) announcement a touch swipe makes, so the shell needs no new
// dispatch: `overview` already reaches the gather overview.
class PointerCornerGesture final : public QObject {
    Q_OBJECT

public:
    explicit PointerCornerGesture(PointerCornerReserver &reserver,
                                  QObject *parent = nullptr);
    ~PointerCornerGesture() override;

    PointerCornerGesture(const PointerCornerGesture &) = delete;
    PointerCornerGesture &operator=(const PointerCornerGesture &) = delete;

    // KWin rebuilds its edge objects when outputs change and carries over
    // only the reservations the old edges held, so the plugin re-arms on
    // every outputs change exactly as it does for the touch edges. KWin
    // ignores a duplicate reservation.
    void rearm();
    [[nodiscard]] bool reserved() const noexcept { return m_reserved; }

    // The edge name and action this corner announces. Public so a test can
    // assert the shell's dispatch and this producer agree on the strings.
    [[nodiscard]] static QString edgeName();
    [[nodiscard]] static QString action();

public Q_SLOTS:
    // KWin's edge callback shape: it passes the border and reads the return
    // value as "was this consumed". Always true - the corner is ours now.
    bool cornerTriggered();

Q_SIGNALS:
    void triggered(const QString &edge, const QString &action);

private:
    PointerCornerReserver &m_reserver;
    bool m_reserved = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
