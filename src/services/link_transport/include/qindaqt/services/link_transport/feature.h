// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_protocol/envelope.h>

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace QindaQt::Link {

class Session;

// A feature plugs into the state machine by owning one frame namespace (the
// text before the first '.', or the whole type when it has no dot).
//
// AGENT-CONTRACT: handle() is called only for sessions in READY state, on the
// session's thread, with a decoded envelope whose namespace matches ns(). The
// handler replies through Session::send(); it must not keep the Session
// reference past sessionClosed(). Handlers must not log vault.* bodies.
class FeatureHandler {
public:
    virtual ~FeatureHandler() = default;
    [[nodiscard]] virtual QString ns() const = 0;
    virtual void handle(Session &session, const Envelope &envelope) = 0;
    virtual void sessionClosed(const QString &deviceId) { Q_UNUSED(deviceId) }
};

// Non-owning registry; the owner outlives every session.
class FeatureRouter {
public:
    void add(FeatureHandler *handler) { m_handlers.insert(handler->ns(), handler); }
    [[nodiscard]] FeatureHandler *find(const QString &frameType) const
    {
        return m_handlers.value(frameType.left(frameType.indexOf(QLatin1Char('.')) < 0
                                                   ? frameType.size()
                                                   : frameType.indexOf(QLatin1Char('.'))),
                                nullptr);
    }
    // Capability strings for our hello: one per registered namespace.
    [[nodiscard]] QStringList caps() const
    {
        QStringList out = m_handlers.keys();
        out.sort();
        return out;
    }
    void notifyClosed(const QString &deviceId) const
    {
        for (FeatureHandler *h : m_handlers) {
            h->sessionClosed(deviceId);
        }
    }

private:
    QHash<QString, FeatureHandler *> m_handlers;
};

} // namespace QindaQt::Link
