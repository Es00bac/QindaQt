// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QMap>
#include <QObject>
#include <QString>

#include <optional>

namespace QindaQt::Obs {

// Where the obs-websocket password lives.
//
// AGENT-CONTRACT: Exactly one secret, under one set of attributes. This is
// not a general secrets API and must not become one: a route that needs a
// different secret gets its own scoped store, so no surface can read a
// credential it was not built to hold.
//
// AGENT-GUARD: A store that cannot reach the Secret Service reports an error.
// It never falls back to a file: a password written beside OBS's config in
// the clear would be a worse promise than "the keyring is not available".
class ObsSecretStore : public QObject {
    Q_OBJECT
public:
    explicit ObsSecretStore(QObject *parent = nullptr) : QObject(parent) {}
    ~ObsSecretStore() override = default;

    // `nullopt` with an empty error means "no secret stored yet", which is
    // the ordinary first-run state. `nullopt` with a diagnostic means the
    // keyring could not be reached and the caller must not generate a new
    // password over the top of one it simply could not read.
    [[nodiscard]] virtual std::optional<QString> password(QString *error) const = 0;
    [[nodiscard]] virtual bool setPassword(const QString &password,
                                           QString *error) = 0;
};

// Production store over org.freedesktop.secrets (gnome-keyring on this
// desktop), default collection, plain session transfer on the session bus.
class SecretServiceObsStore final : public ObsSecretStore {
    Q_OBJECT
public:
    explicit SecretServiceObsStore(QDBusConnection bus,
                                   QObject *parent = nullptr);

    [[nodiscard]] std::optional<QString> password(QString *error) const override;
    [[nodiscard]] bool setPassword(const QString &password,
                                   QString *error) override;

    // The attributes this store searches and writes under.
    //
    // AGENT-GUARD: a{ss}, not a{sv}. The Secret Service spec types both
    // SearchItems' argument and the item's Attributes property as a string
    // map; sending a QVariantMap marshals as a{sv} and the keyring rejects
    // or silently stores nothing searchable.
    [[nodiscard]] static QMap<QString, QString> attributes();

private:
    QDBusConnection m_bus;
};

} // namespace QindaQt::Obs
