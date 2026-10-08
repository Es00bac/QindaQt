// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "voice_configuration_types.h"
#include <QtCore/QObject>
#include <QtCore/QVariantMap>
namespace QindaQt::Services::VoiceConfiguration {
// Same-thread borrowed transport outlives its client. start() only discovers
// an existing Voice1 owner: activation is forbidden (ADR0364). Replies retain
// the initiating owner/token; Qt transport additionally validates wire sender.
class Transport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fetch(const QString &owner, quint64 token) = 0;
    virtual void submit(const QString &owner, quint64 token, quint64 request,
                        quint64 revision, Operation operation, const QString &key) = 0;
Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner);
    void snapshotReply(const QString &owner, quint64 token, bool success,
                       bool unsupported, const QVariantMap &map);
    void operationReply(const QString &owner, quint64 token, bool success,
                        const QVariantMap &map);
};
}
