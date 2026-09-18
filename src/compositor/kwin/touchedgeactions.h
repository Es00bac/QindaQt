// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <array>
#include <memory>

class QAction;

namespace QindaQt::Compositor::KWinIntegration {

// The four touch edges and the QindaQt action a swipe from each one asks
// for (ADR-0205). Pure data, so the mapping and the settings decoding are
// testable without a compositor.
enum class TouchEdge { Left, Top, Right, Bottom };

[[nodiscard]] QString touchEdgeName(TouchEdge edge);
[[nodiscard]] constexpr std::array<TouchEdge, 4> allTouchEdges()
{
    return {TouchEdge::Left, TouchEdge::Top, TouchEdge::Right, TouchEdge::Bottom};
}

struct TouchEdgeActions {
    QString left = QStringLiteral("overview");
    QString top = QStringLiteral("notifications");
    QString right = QStringLiteral("none");
    QString bottom = QStringLiteral("task-switcher");

    [[nodiscard]] static QStringList knownActions();
    [[nodiscard]] static QString settingsKey(TouchEdge edge);
    [[nodiscard]] static TouchEdgeActions fromSettingsValues(const QVariantMap &values);
    [[nodiscard]] QString actionFor(TouchEdge edge) const;
    void setActionFor(TouchEdge edge, const QString &action);
    bool operator==(const TouchEdgeActions &other) const = default;
};

// Everything the compositor reads from `input.touch.*`.
struct TouchPreferences {
    int longPressMs = 500;
    bool touchscreenEnabled = true;
    // auto: the compositor's own rule (a finger or pen focused the field);
    // off: the keyboard is not started.
    QString onScreenKeyboard = QStringLiteral("auto");
    TouchEdgeActions edges;

    [[nodiscard]] static QStringList settingsKeys();
    [[nodiscard]] static TouchPreferences fromSettingsValues(const QVariantMap &values);
    bool operator==(const TouchPreferences &other) const = default;
};

// The seam to the compositor's screen edges: KWin in production, a recorder
// in tests.
class TouchEdgeReserver {
public:
    virtual ~TouchEdgeReserver();
    virtual void reserve(TouchEdge edge, QAction *action) = 0;
    virtual void unreserve(TouchEdge edge, QAction *action) = 0;
};

// Owns one QAction per edge, reserves the edges whose action is not `none`,
// and announces a completed swipe as (edge, action).
class TouchEdgeGestures final : public QObject {
    Q_OBJECT

public:
    explicit TouchEdgeGestures(TouchEdgeReserver &reserver, QObject *parent = nullptr);
    ~TouchEdgeGestures() override;

    void apply(const TouchEdgeActions &actions);
    // Reserves every wanted edge again. KWin rebuilds its edge objects when
    // outputs change and only carries reservations over from edges that
    // existed; a reservation made before the first edges exist is lost, so
    // the plugin re-arms on every outputs change. KWin ignores a duplicate.
    void rearm();
    [[nodiscard]] const TouchEdgeActions &actions() const { return m_actions; }
    [[nodiscard]] QAction *actionObject(TouchEdge edge) const;

Q_SIGNALS:
    void triggered(const QString &edge, const QString &action);

private:
    TouchEdgeReserver &m_reserver;
    TouchEdgeActions m_actions;
    std::array<std::unique_ptr<QAction>, 4> m_edgeActions;
    std::array<bool, 4> m_reserved{};
};

} // namespace QindaQt::Compositor::KWinIntegration
