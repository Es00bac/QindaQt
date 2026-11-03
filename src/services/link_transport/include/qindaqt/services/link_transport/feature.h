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
    // Further frame namespaces served by this handler without a capability of their
    // own (`desktop.*` rides on the `vault` capability). Advertised caps stay ns() only.
    [[nodiscard]] virtual QStringList extraNamespaces() const { return {}; }
    virtual void handle(Session &session, const Envelope &envelope) = 0;
    virtual void sessionClosed(const QString &deviceId) { Q_UNUSED(deviceId) }
};

// Non-owning registry; the owner outlives every session.
class FeatureRouter {
public:
    void add(FeatureHandler *handler)
    {
        m_handlers.insert(handler->ns(), handler);
        for (const QString &extra : handler->extraNamespaces()) m_aliases.insert(extra, handler);
    }
    [[nodiscard]] FeatureHandler *find(const QString &frameType) const
    {
        const QString prefix = frameType.left(frameType.indexOf(QLatin1Char('.')) < 0
                                                  ? frameType.size()
                                                  : frameType.indexOf(QLatin1Char('.')));
        FeatureHandler *h = m_handlers.value(prefix, nullptr);
        return h != nullptr ? h : m_aliases.value(prefix, nullptr);
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
    QHash<QString, FeatureHandler *> m_aliases;
};

} // namespace QindaQt::Link
