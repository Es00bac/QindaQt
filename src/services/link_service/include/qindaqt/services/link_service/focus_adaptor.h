// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_focus/focus_store.h>
#include <qindaqt/services/link_transport/caller_authority.h>

#include <QtCore/QString>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusServiceWatcher>

namespace QindaQt::Link {

// org.qindaqt.Link1.Focus on /org/qindaqt/Link1 (ADR-0440): the shell tells the service which window
// has focus. The service cannot learn that itself: the authenticated identity belongs to the panel owner
// (ADR-0063), which is the shell.
//
// `Publish(s json)` is accepted only from the shell (CallerRole::Shell); everyone else, including
// the QindaLink app, gets org.qindaqt.Link1.Error.Denied. Malformed or oversized JSON gets
// org.qindaqt.Link1.Error.BadRequest and leaves the stored fact untouched. When the connection that
// published last leaves the bus the fact is cleared, so a dead shell never leaves a stale target.
// AGENT-GUARD: the title is display text from a window; it is never logged here.
class FocusAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Link1.Focus")
public:
    // `store` and `authority` are borrowed; `exported` is the object the adaptor is added to.
    FocusAdaptor(FocusStore &store, QDBusConnection bus, QObject *exported, CallerAuthority *authority);

public Q_SLOTS:
    void Publish(const QString &json, const QDBusMessage &msg);

private:
    FocusStore &m_store;
    CallerAuthority *m_authority;
    QDBusConnection m_bus;
    QDBusServiceWatcher m_watcher;
    QString m_publisher;
};

} // namespace QindaQt::Link
