// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QVariantList>
#include <qindaqt/services/keyring/secure_buffer.h>
#include <memory>
namespace QindaQt::Services::KeyringClient {
using SecretBytes=std::shared_ptr<qindaqt::keyring::SecureBuffer>;
enum class Request { Collections, Items, Lock, Unlock, Create, ChangePassword, Reveal, Delete };
// GUI-thread async boundary. Caller retains gateway; it outlives its model.
// One admitted request at a time, nonzero caller token echoed exactly. cancel()
// fences late replies and dismisses an owned prompt; no mutation is replayed.
// Metadata maps use lower-case keys. indexAuthenticated is disclosure metadata,
// never authorization. Secret ownership is shared only across direct same-thread
// delivery, then retained by the consumer until explicit clear; no implicit text.
class KeyringGateway : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool available() const = 0;
    virtual void request(quint64 token, Request kind, const QString &path = {},
                         const QString &label = {}) = 0;
    virtual void cancel() = 0;
Q_SIGNALS:
    void authorityChanged();
    void secretsInvalidated();
    void metadataChanged();
    // Fixed Keyring1 policy map; ScreenLocked includes native Unknown.
    // Reveal/copy admission always requires available, admitted Unlocked,
    // independently of the collection-lock preference.
    void policyChanged(const QVariantMap &state);
    void rowsReady(quint64 token, const QVariantList &rows);
    void actionFinished(quint64 token, bool confirmed, const QString &message);
    void secretReady(quint64 token, std::shared_ptr<qindaqt::keyring::SecureBuffer> bytes,
                     const QString &contentType);
};
}

Q_DECLARE_METATYPE(QindaQt::Services::KeyringClient::SecretBytes)
