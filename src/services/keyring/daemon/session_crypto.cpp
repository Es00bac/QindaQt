// SPDX-License-Identifier: GPL-3.0-or-later
#include "session_crypto.h"
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/dh.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/param_build.h>
#include <openssl/rand.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace qindaqt::keyring::service {
namespace {
[[noreturn]] void fail() { throw std::runtime_error("Invalid session exchange"); }
template<class T, void(*F)(T*)> using Owned = std::unique_ptr<T, decltype(F)>;
Owned<EVP_PKEY, EVP_PKEY_free> importDh(const BIGNUM *p, const BIGNUM *g, const BIGNUM *pub = nullptr) {
    Owned<OSSL_PARAM_BLD, OSSL_PARAM_BLD_free> b(OSSL_PARAM_BLD_new(), OSSL_PARAM_BLD_free);
    if (!b || !OSSL_PARAM_BLD_push_BN(b.get(), OSSL_PKEY_PARAM_FFC_P, p)
        || !OSSL_PARAM_BLD_push_BN(b.get(), OSSL_PKEY_PARAM_FFC_G, g)
        || (pub && !OSSL_PARAM_BLD_push_BN(b.get(), OSSL_PKEY_PARAM_PUB_KEY, pub))) fail();
    Owned<OSSL_PARAM, OSSL_PARAM_free> params(OSSL_PARAM_BLD_to_param(b.get()), OSSL_PARAM_free);
    Owned<EVP_PKEY_CTX, EVP_PKEY_CTX_free> ctx(EVP_PKEY_CTX_new_from_name(nullptr, "DH", nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY *key = nullptr;
    if (!params || !ctx || EVP_PKEY_fromdata_init(ctx.get()) != 1
        || EVP_PKEY_fromdata(ctx.get(), &key, pub ? EVP_PKEY_PUBLIC_KEY : EVP_PKEY_KEY_PARAMETERS, params.get()) != 1) fail();
    return {key, EVP_PKEY_free};
}
}
QByteArray SessionCrypto::negotiate(const QString &algorithm, const QByteArray &input) {
    key_.clear(); encrypted_ = false;
    if (algorithm == "plain") {
        if (!input.isEmpty()) fail();
        return {};
    }
    if (algorithm != "dh-ietf1024-sha256-aes128-cbc-pkcs7" || input.isEmpty() || input.size() > 128) fail();
    // AGENT-CONTRACT: Secret Service specifies RFC2409 group 2 and padded
    // 128-byte shared input to HKDF-SHA256, NULL salt, empty info, AES128-CBC.
    Owned<BIGNUM, BN_free> p(BN_get_rfc2409_prime_1024(nullptr), BN_free), g(BN_new(), BN_free);
    Owned<BIGNUM, BN_free> peerBn(BN_bin2bn(reinterpret_cast<const unsigned char *>(input.constData()),
                                        static_cast<int>(input.size()), nullptr), BN_free);
    if (!p || !g || !peerBn || BN_set_word(g.get(), 2) != 1
        || BN_cmp(peerBn.get(), BN_value_one()) <= 0 || BN_cmp(peerBn.get(), p.get()) >= 0) fail();
    auto parameters = importDh(p.get(), g.get());
    Owned<EVP_PKEY_CTX, EVP_PKEY_CTX_free> generation(EVP_PKEY_CTX_new(parameters.get(), nullptr), EVP_PKEY_CTX_free);
    EVP_PKEY *generated = nullptr;
    if (!generation || EVP_PKEY_keygen_init(generation.get()) != 1
        || EVP_PKEY_generate(generation.get(), &generated) != 1) fail();
    Owned<EVP_PKEY, EVP_PKEY_free> privateKey(generated, EVP_PKEY_free);
    auto peer = importDh(p.get(), g.get(), peerBn.get());
    Owned<EVP_PKEY_CTX, EVP_PKEY_CTX_free> derive(EVP_PKEY_CTX_new(privateKey.get(), nullptr), EVP_PKEY_CTX_free);
    SecureBuffer shared(128);
    std::size_t length = shared.size();
    if (!derive || EVP_PKEY_derive_init(derive.get()) != 1
        || EVP_PKEY_CTX_set_dh_pad(derive.get(), 1) != 1
        || EVP_PKEY_derive_set_peer_ex(derive.get(), peer.get(), 1) != 1
        || EVP_PKEY_derive(derive.get(), shared.bytes().data(), &length) != 1 || length != 128) fail();
    Owned<EVP_KDF, EVP_KDF_free> kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr), EVP_KDF_free);
    Owned<EVP_KDF_CTX, EVP_KDF_CTX_free> hkdf(kdf ? EVP_KDF_CTX_new(kdf.get()) : nullptr, EVP_KDF_CTX_free);
    char digest[] = "SHA256";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, shared.bytes().data(), shared.size()),
        OSSL_PARAM_construct_end()
    };
    SecureBuffer key(16);
    if (!hkdf || EVP_KDF_derive(hkdf.get(), key.bytes().data(), key.size(), params) != 1) fail();
    BIGNUM *pub = nullptr;
    if (EVP_PKEY_get_bn_param(privateKey.get(), OSSL_PKEY_PARAM_PUB_KEY, &pub) != 1) fail();
    Owned<BIGNUM, BN_free> publicKey(pub, BN_free);
    QByteArray output(128, Qt::Uninitialized);
    if (BN_bn2binpad(pub, reinterpret_cast<unsigned char *>(output.data()), 128) != 128) fail();
    key_ = std::move(key); encrypted_ = true;
    return output;
}
SecureBuffer SessionCrypto::decode(WireSecret &wire, std::size_t maximum) const {
    if (maximum > 1024 * 1024 || wire.value.size() > static_cast<qsizetype>(maximum + 16)
        || wire.contentType.isEmpty() || wire.contentType.size() > 128 || wire.contentType.toUtf8().size() > 128) fail();
    if (!encrypted_) {
        if (!wire.parameters.isEmpty() || wire.value.size() > static_cast<qsizetype>(maximum)) fail();
        SecureBuffer plain(static_cast<std::size_t>(wire.value.size()));
        std::copy(wire.value.begin(), wire.value.end(), plain.bytes().begin());
        wipe(wire.value); return plain;
    }
    if (wire.parameters.size() != 16 || wire.value.isEmpty() || wire.value.size() % 16 != 0) fail();
    Owned<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    SecureBuffer staging(static_cast<std::size_t>(wire.value.size()) + 16);
    int count = 0, final = 0;
    if (!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, key_.bytes().data(),
                                 reinterpret_cast<const unsigned char *>(wire.parameters.constData())) != 1
        || EVP_DecryptUpdate(ctx.get(), staging.bytes().data(), &count,
            reinterpret_cast<const unsigned char *>(wire.value.constData()), static_cast<int>(wire.value.size())) != 1
        || EVP_DecryptFinal_ex(ctx.get(), staging.bytes().data() + count, &final) != 1
        || static_cast<std::size_t>(count + final) > maximum) fail();
    SecureBuffer plain(static_cast<std::size_t>(count + final));
    std::copy_n(staging.bytes().begin(), plain.size(), plain.bytes().begin());
    return plain;
}
WireSecret SessionCrypto::encode(const QDBusObjectPath &session, const SecureBuffer &secret, const QString &type) const {
    WireSecret result{session, {}, {}, type};
    if (!encrypted_) {
        result.value = QByteArray(reinterpret_cast<const char *>(secret.bytes().data()), static_cast<qsizetype>(secret.size()));
        return result;
    }
    result.parameters.resize(16);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(result.parameters.data()), 16) != 1) fail();
    result.value.resize(static_cast<qsizetype>(secret.size()) + 16);
    Owned<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int count = 0, final = 0;
    if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, key_.bytes().data(),
                                 reinterpret_cast<const unsigned char *>(result.parameters.constData())) != 1
        || EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(result.value.data()), &count,
                             secret.bytes().data(), static_cast<int>(secret.size())) != 1
        || EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(result.value.data()) + count, &final) != 1) fail();
    result.value.resize(count + final); return result;
}
}
