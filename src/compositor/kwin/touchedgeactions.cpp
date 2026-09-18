// SPDX-License-Identifier: GPL-3.0-or-later
#include "touchedgeactions.h"

#include <QAction>
#include <algorithm>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

constexpr auto LongPressKey = "input.touch.longPressMs";
constexpr auto EnabledKey = "input.touch.enabled";
constexpr auto KeyboardKey = "input.touch.onScreenKeyboard";
constexpr int MinimumLongPressMs = 200;
constexpr int MaximumLongPressMs = 1500;

std::size_t index(TouchEdge edge)
{
    return static_cast<std::size_t>(edge);
}

} // namespace

QString touchEdgeName(TouchEdge edge)
{
    switch (edge) {
    case TouchEdge::Left: return QStringLiteral("left");
    case TouchEdge::Top: return QStringLiteral("top");
    case TouchEdge::Right: return QStringLiteral("right");
    case TouchEdge::Bottom: return QStringLiteral("bottom");
    }
    return QString();
}

QStringList TouchEdgeActions::knownActions()
{
    return {QStringLiteral("none"), QStringLiteral("overview"), QStringLiteral("notifications"),
            QStringLiteral("task-switcher")};
}

QString TouchEdgeActions::settingsKey(TouchEdge edge)
{
    switch (edge) {
    case TouchEdge::Left: return QStringLiteral("input.touch.edgeLeft");
    case TouchEdge::Top: return QStringLiteral("input.touch.edgeTop");
    case TouchEdge::Right: return QStringLiteral("input.touch.edgeRight");
    case TouchEdge::Bottom: return QStringLiteral("input.touch.edgeBottom");
    }
    return QString();
}

QString TouchEdgeActions::actionFor(TouchEdge edge) const
{
    switch (edge) {
    case TouchEdge::Left: return left;
    case TouchEdge::Top: return top;
    case TouchEdge::Right: return right;
    case TouchEdge::Bottom: return bottom;
    }
    return QStringLiteral("none");
}

void TouchEdgeActions::setActionFor(TouchEdge edge, const QString &action)
{
    const QString value = knownActions().contains(action) ? action : QStringLiteral("none");
    switch (edge) {
    case TouchEdge::Left: left = value; return;
    case TouchEdge::Top: top = value; return;
    case TouchEdge::Right: right = value; return;
    case TouchEdge::Bottom: bottom = value; return;
    }
}

TouchEdgeActions TouchEdgeActions::fromSettingsValues(const QVariantMap &values)
{
    TouchEdgeActions actions;
    for (const TouchEdge edge : allTouchEdges()) {
        const QVariant value = values.value(settingsKey(edge));
        if (value.isValid() && value.canConvert<QString>()) {
            actions.setActionFor(edge, value.toString());
        }
    }
    return actions;
}

QStringList TouchPreferences::settingsKeys()
{
    QStringList keys{QString::fromLatin1(LongPressKey), QString::fromLatin1(EnabledKey), QString::fromLatin1(KeyboardKey)};
    for (const TouchEdge edge : allTouchEdges()) {
        keys.append(TouchEdgeActions::settingsKey(edge));
    }
    return keys;
}

TouchPreferences TouchPreferences::fromSettingsValues(const QVariantMap &values)
{
    TouchPreferences preferences;
    const QVariant longPress = values.value(QString::fromLatin1(LongPressKey));
    if (longPress.isValid()) {
        bool ok = false;
        const int milliseconds = longPress.toInt(&ok);
        if (ok) {
            preferences.longPressMs = std::clamp(milliseconds, MinimumLongPressMs, MaximumLongPressMs);
        }
    }
    const QVariant enabled = values.value(QString::fromLatin1(EnabledKey));
    if (enabled.isValid() && enabled.canConvert<bool>()) {
        preferences.touchscreenEnabled = enabled.toBool();
    }
    const QString keyboard = values.value(QString::fromLatin1(KeyboardKey)).toString();
    preferences.onScreenKeyboard = keyboard == QLatin1String("off") ? QStringLiteral("off") : QStringLiteral("auto");
    preferences.edges = TouchEdgeActions::fromSettingsValues(values);
    return preferences;
}

TouchEdgeReserver::~TouchEdgeReserver() = default;

TouchEdgeGestures::TouchEdgeGestures(TouchEdgeReserver &reserver, QObject *parent)
    : QObject(parent), m_reserver(reserver)
{
    for (const TouchEdge edge : allTouchEdges()) {
        auto action = std::make_unique<QAction>(this);
        action->setObjectName(QStringLiteral("qindaqt.touch-edge.") + touchEdgeName(edge));
        connect(action.get(), &QAction::triggered, this, [this, edge] {
            const QString name = m_actions.actionFor(edge);
            if (name != QLatin1String("none")) {
                Q_EMIT triggered(touchEdgeName(edge), name);
            }
        });
        m_edgeActions[index(edge)] = std::move(action);
    }
    // Every edge starts unreserved; apply() reserves the ones with an action.
    m_actions = TouchEdgeActions{QStringLiteral("none"), QStringLiteral("none"), QStringLiteral("none"),
                                 QStringLiteral("none")};
}

TouchEdgeGestures::~TouchEdgeGestures()
{
    for (const TouchEdge edge : allTouchEdges()) {
        if (m_reserved[index(edge)]) {
            m_reserver.unreserve(edge, m_edgeActions[index(edge)].get());
        }
    }
}

QAction *TouchEdgeGestures::actionObject(TouchEdge edge) const
{
    return m_edgeActions[index(edge)].get();
}

void TouchEdgeGestures::rearm()
{
    for (const TouchEdge edge : allTouchEdges()) {
        if (m_reserved[index(edge)]) {
            m_reserver.reserve(edge, m_edgeActions[index(edge)].get());
        }
    }
}

void TouchEdgeGestures::apply(const TouchEdgeActions &actions)
{
    for (const TouchEdge edge : allTouchEdges()) {
        const bool wanted = actions.actionFor(edge) != QLatin1String("none");
        const std::size_t at = index(edge);
        if (wanted && !m_reserved[at]) {
            m_reserver.reserve(edge, m_edgeActions[at].get());
            m_reserved[at] = true;
        } else if (!wanted && m_reserved[at]) {
            m_reserver.unreserve(edge, m_edgeActions[at].get());
            m_reserved[at] = false;
        }
    }
    m_actions = actions;
}

} // namespace QindaQt::Compositor::KWinIntegration
