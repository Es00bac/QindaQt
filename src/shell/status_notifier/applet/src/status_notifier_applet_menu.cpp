// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_source_interface.h"
#include <QtCore/QScopeGuard>

namespace QindaQt::StatusNotifierApplet {

bool StatusNotifierAppletController::itemIsMenu(const QString &owner, const QString &path,
                                               quint64 generation) const
{
    return liveTarget(owner, path, generation)
        && m_source->itemIsMenu({owner, path, generation});
}

bool StatusNotifierAppletController::hasExportedMenu(const QString &owner, const QString &path,
                                                    quint64 generation) const
{
    return liveTarget(owner, path, generation)
        && m_source->hasExportedMenu({owner, path, generation});
}

QVariantMap StatusNotifierAppletController::menuStateFor(const QString &owner, const QString &path,
                                                        quint64 generation) const
{
    if (!liveTarget(owner, path, generation)) {
        return {{QStringLiteral("status"), QStringLiteral("none")}};
    }
    return m_source->menuState({owner, path, generation});
}

bool StatusNotifierAppletController::dispatchMenu(const QString &owner, const QString &path,
                                                 quint64 generation, const QString &revision,
                                                 int itemId, bool opening)
{
    if (!admitAction(owner, path, generation)) {
        return false;
    }
    bool valid = false;
    const quint64 serial = revision.toULongLong(&valid);
    if (!valid || serial == 0 || QString::number(serial) != revision || itemId < 0) {
        setFeedback(m_texts.feedbackStaleItem);
        return false;
    }
    m_dispatchInProgress = true;
    const auto guard = qScopeGuard([this] { m_dispatchInProgress = false; });
    // AGENT-GUARD: send the captured revision unchanged. Substituting the current
    // revision here would authorize a click queued against a replaced menu.
    const QindaQt::StatusNotifier::OwnerKey target {owner, path, generation};
    const auto outcome = opening ? m_source->aboutToShowMenu(target, serial, itemId)
                                 : m_source->invokeMenu(target, serial, itemId);
    if (!outcome.accepted()) {
        setFeedback(m_texts.feedbackRefused.arg(outcome.reasonCode));
    }
    return outcome.accepted();
}

bool StatusNotifierAppletController::invokeMenu(const QString &owner, const QString &path,
                                               quint64 generation, const QString &revision,
                                               int itemId)
{
    return dispatchMenu(owner, path, generation, revision, itemId, false);
}

bool StatusNotifierAppletController::aboutToShowMenu(const QString &owner, const QString &path,
                                                    quint64 generation, const QString &revision,
                                                    int itemId)
{
    return dispatchMenu(owner, path, generation, revision, itemId, true);
}

bool StatusNotifierAppletController::scrollItem(const QString &owner, const QString &path,
                                               quint64 generation, int delta,
                                               const QString &orientation)
{
    if (!admitAction(owner, path, generation)) {
        return false;
    }
    m_dispatchInProgress = true;
    const auto guard = qScopeGuard([this] { m_dispatchInProgress = false; });
    const auto outcome = m_source->scroll({owner, path, generation}, delta, orientation);
    if (!outcome.accepted()) {
        setFeedback(m_texts.feedbackRefused.arg(outcome.reasonCode));
    }
    return outcome.accepted();
}

} // namespace QindaQt::StatusNotifierApplet
