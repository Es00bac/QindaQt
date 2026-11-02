// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtCore/QString>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>

namespace QindaQt::Link {

// A TLS identity: ECDSA P-256 key plus self-signed certificate.
struct DeviceIdentity {
    // QSslCertificate's default constructor is explicit, so spell it out.
    DeviceIdentity() : certificate(QByteArray(), QSsl::Pem) {}
    QSslKey key;
    QSslCertificate certificate;
    [[nodiscard]] bool isValid() const { return !key.isNull() && !certificate.isNull(); }
    // Raw 32-byte SHA-256 of the DER SubjectPublicKeyInfo.
    [[nodiscard]] QByteArray spkiDigest() const;
};

// Raw 32-byte pin of `certificate`; empty when it is null.
[[nodiscard]] QByteArray spkiDigest(const QSslCertificate &certificate);
// The deviceId form: 64 lowercase hex characters.
[[nodiscard]] QString deviceIdFor(const QSslCertificate &certificate);

// Generates a fresh identity with libcrypto. Invalid result on failure.
[[nodiscard]] DeviceIdentity generateIdentity(const QString &commonName);

// Persists the identity as PEM inside an injected directory.
//
// AGENT-GUARD: the directory is created 0700 and the key file 0600. Callers
// inject the directory; production passes $XDG_DATA_HOME/qindalink and tests
// pass a QTemporaryDir. Nothing here ever reads a default location.
class IdentityStore {
public:
    explicit IdentityStore(QString directory) : m_directory(std::move(directory)) {}
    // Loads the existing identity, or creates and saves a new one.
    [[nodiscard]] DeviceIdentity loadOrCreate(const QString &commonName, QString *error = nullptr) const;
    [[nodiscard]] DeviceIdentity load() const;
    [[nodiscard]] bool save(const DeviceIdentity &identity, QString *error = nullptr) const;

private:
    QString m_directory;
};

} // namespace QindaQt::Link
