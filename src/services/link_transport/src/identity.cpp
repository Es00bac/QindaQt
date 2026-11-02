// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/identity.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QSaveFile>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>

#include <memory>

namespace QindaQt::Link {
namespace {

template <auto Fn> struct Deleter { template <class T> void operator()(T *p) const { Fn(p); } };
using PkeyPtr = std::unique_ptr<EVP_PKEY, Deleter<EVP_PKEY_free>>;
using X509Ptr = std::unique_ptr<X509, Deleter<X509_free>>;
using BioPtr = std::unique_ptr<BIO, Deleter<BIO_free_all>>;

QByteArray bioBytes(BIO *bio)
{
    char *data = nullptr;
    const long n = BIO_get_mem_data(bio, &data);
    return n > 0 ? QByteArray(data, qsizetype(n)) : QByteArray();
}

QString filePath(const QString &dir, const char *name) { return QDir(dir).filePath(QLatin1String(name)); }

bool writeSecure(const QString &path, const QByteArray &bytes, QFileDevice::Permissions perms)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return false;
    }
    f.setPermissions(perms);
    f.write(bytes);
    return f.commit();
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

QByteArray spkiDigest(const QSslCertificate &certificate)
{
    if (certificate.isNull()) {
        return {};
    }
    // QSslKey::toDer() on a public key is the DER SubjectPublicKeyInfo.
    return QCryptographicHash::hash(certificate.publicKey().toDer(), QCryptographicHash::Sha256);
}

QString deviceIdFor(const QSslCertificate &certificate)
{
    return QString::fromLatin1(spkiDigest(certificate).toHex());
}

QByteArray DeviceIdentity::spkiDigest() const { return QindaQt::Link::spkiDigest(certificate); }

DeviceIdentity generateIdentity(const QString &commonName)
{
    PkeyPtr key(EVP_EC_gen("P-256"));
    X509Ptr cert(X509_new());
    if (!key || !cert) {
        return {};
    }
    unsigned char serial[8];
    if (RAND_bytes(serial, sizeof serial) != 1) {
        return {};
    }
    serial[0] &= 0x7f;
    ASN1_INTEGER *asn1Serial = X509_get_serialNumber(cert.get());
    BIGNUM *bn = BN_bin2bn(serial, sizeof serial, nullptr);
    BN_to_ASN1_INTEGER(bn, asn1Serial);
    BN_free(bn);
    X509_set_version(cert.get(), 2);
    X509_gmtime_adj(X509_getm_notBefore(cert.get()), -86400);
    X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60 * 24 * 365 * 50);
    X509_set_pubkey(cert.get(), key.get());
    X509_NAME *name = X509_get_subject_name(cert.get());
    const QByteArray cn = commonName.toUtf8();
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_UTF8,
                               reinterpret_cast<const unsigned char *>(cn.constData()), int(cn.size()), -1, 0);
    X509_set_issuer_name(cert.get(), name);
    if (X509_sign(cert.get(), key.get(), EVP_sha256()) == 0) {
        return {};
    }
    BioPtr keyBio(BIO_new(BIO_s_mem()));
    BioPtr certBio(BIO_new(BIO_s_mem()));
    if (PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1
        || PEM_write_bio_X509(certBio.get(), cert.get()) != 1) {
        return {};
    }
    DeviceIdentity id;
    id.key = QSslKey(bioBytes(keyBio.get()), QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
    id.certificate = QSslCertificate(bioBytes(certBio.get()), QSsl::Pem);
    return id.isValid() ? id : DeviceIdentity{};
}

DeviceIdentity IdentityStore::load() const
{
    DeviceIdentity id;
    id.key = QSslKey(readAll(filePath(m_directory, "identity-key.pem")), QSsl::Ec, QSsl::Pem,
                     QSsl::PrivateKey);
    id.certificate = QSslCertificate(readAll(filePath(m_directory, "identity-cert.pem")), QSsl::Pem);
    return id.isValid() ? id : DeviceIdentity{};
}

bool IdentityStore::save(const DeviceIdentity &identity, QString *error) const
{
    auto fail = [&](const char *why) {
        if (error != nullptr) {
            *error = QLatin1String(why);
        }
        return false;
    };
    if (!identity.isValid()) {
        return fail("identity is not valid");
    }
    if (!QDir().mkpath(m_directory)) {
        return fail("cannot create identity directory");
    }
    QFile::setPermissions(m_directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if (!writeSecure(filePath(m_directory, "identity-key.pem"), identity.key.toPem(),
                     QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || !writeSecure(filePath(m_directory, "identity-cert.pem"), identity.certificate.toPem(),
                        QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        return fail("cannot write identity files");
    }
    return true;
}

DeviceIdentity IdentityStore::loadOrCreate(const QString &commonName, QString *error) const
{
    if (DeviceIdentity existing = load(); existing.isValid()) {
        return existing;
    }
    DeviceIdentity fresh = generateIdentity(commonName);
    if (!fresh.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral("identity generation failed");
        }
        return {};
    }
    return save(fresh, error) ? fresh : DeviceIdentity{};
}

} // namespace QindaQt::Link
