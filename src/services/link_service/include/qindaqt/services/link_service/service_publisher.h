// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Link {

struct ServiceAdvert {
    QString name;                  // instance name; the publisher may rename it on a collision
    QString type;                  // e.g. "_qindalink._tcp"
    quint16 port = 0;
    QMap<QString, QString> txt;
};

// Seam over mDNS publication so LanController is testable without Avahi.
//
// Ownership: the caller keeps the publisher alive for the controller's life.
// Threading: one thread. Error behavior: publish() never throws or blocks the
// caller; failures surface through state() and stateChanged().
// AGENT-CONTRACT: state() is one of "off" (nothing published or asked for),
// "published", "renamed" (published after a name collision), "unavailable"
// (the mDNS daemon is absent; publication resumes when it returns) or "failed".
// withdraw() must remove the record from the network before returning.
class ServicePublisher : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void publish(const ServiceAdvert &advert) = 0;
    virtual void withdraw() = 0;
    [[nodiscard]] virtual QString state() const = 0;

Q_SIGNALS:
    void stateChanged(const QString &state);
};

} // namespace QindaQt::Link
