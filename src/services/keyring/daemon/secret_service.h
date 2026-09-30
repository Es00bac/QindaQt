// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "collection_repository.h"
#include "session_crypto.h"
#include "prompt_provider.h"
#include "lock_policy.h"
#include <QPointer>
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <map>

namespace qindaqt::keyring::service {
struct Session { QString owner; SessionCrypto crypto; };
struct Prompt {
    QString owner, action, label, alias;
    QStringList collections;
    Paths objects, completed;
    qsizetype next = 0;
    quint64 ticket = 0;
    bool running = false, relockOnCancel = false;
    QString secretSession, portalApplication;
};
class SecretService final : public QDBusVirtualObject {
    Q_OBJECT
public:
    SecretService(CollectionRepository &, PromptProvider &, QDBusConnection, QObject *parent = nullptr);
    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &, const QDBusConnection &) override;
    void notifyCollectionState(const QString &id);
    // Borrows same-thread QObject, auto-fenced on destruction. Native reveal
    // remains unavailable without this independently admitted observer.
    void observeLockPolicy(KeyringLockPolicy *);
    QString collectionPath(const QString &id) const;
    QString itemPath(const QString &id, const QString &item) const;
    QString collectionForPath(const QString &path) const;
private Q_SLOTS:
    void ownerLost(const QString &name, const QString &oldOwner, const QString &newOwner);
private:
    bool serviceMethod(const QDBusMessage &);
    bool collectionMethod(const QDBusMessage &, const QString &id);
    bool itemMethod(const QDBusMessage &, const QString &id, const QString &item);
    bool propertyMethod(const QDBusMessage &);
    bool nativeMethod(const QDBusMessage &);
    bool nativeItemMethod(const QDBusMessage &);
    bool portalMethod(const QDBusMessage &);
    bool portalCaller(const QString &) const;
    SecureBuffer portalSecret(const QString &app);
    void changePromptPassword(const QString &,SecureBuffer);
    bool promptMethod(const QDBusMessage &);
    void startPrompt(const QString &path);
    void finishPrompt(const QString &path, bool dismissed);
    bool nativeDisclosureAllowed() const;
    QString addPrompt(Prompt prompt);
    Session &session(const QString &path, const QString &owner);
    bool exists(const QString &path) const;
    QString itemForPath(const QString &path, const QString &id) const;
    Paths paths(const QString &id, const SearchResult &result) const;
    QVariantMap properties(const QString &path, const QString &interface) const;
    void reply(const QDBusMessage &, const QVariantList &arguments = {});
    void error(const QDBusMessage &, const QString &name);
    void changed(const QString &path, const QString &interface, const QVariantMap &properties);
    void signal(const QString &path, const QString &interface, const QString &name, const QVariantList &arguments);
    CollectionRepository &repository_;
    PromptProvider &promptsProvider_;
    QDBusConnection bus_;
    std::map<QString, std::unique_ptr<Session>> sessions_;
    std::map<QString, Prompt> prompts_;
    QString sessionOwner_;
    QPointer<KeyringLockPolicy> lockPolicy_;
    int pendingRekeys_ = 0;
};
inline QDBusObjectPath objectPath(const QString &path = "/") { return QDBusObjectPath(path); }
inline QVariant variantPath(const QString &path = "/") { return QVariant::fromValue(objectPath(path)); }
constexpr auto Root = "/org/freedesktop/secrets";
constexpr auto ServiceInterface = "org.freedesktop.Secret.Service";
constexpr auto CollectionInterface = "org.freedesktop.Secret.Collection";
constexpr auto ItemInterface = "org.freedesktop.Secret.Item";
constexpr auto NativeInterface = "org.qindaqt.Keyring1";
}
