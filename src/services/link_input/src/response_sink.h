// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

namespace QindaQt::Link {

// Receives one org.freedesktop.portal.Request.Response. Private to the portal
// session: it exists because the Response signal must be subscribed to
// before the method call that triggers it, or a fast portal races us.
class ResponseSink final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
public Q_SLOTS:
    void Response(uint response, const QVariantMap &results)
    {
        if (!m_done) {
            m_done = true;
            Q_EMIT arrived(response, results);
        }
    }
Q_SIGNALS:
    void arrived(uint response, const QVariantMap &results);

private:
    bool m_done = false;
};

} // namespace QindaQt::Link
