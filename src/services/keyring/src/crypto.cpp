// SPDX-License-Identifier: GPL-3.0-or-later
#include "crypto_p.h"
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <memory>
#include <stdexcept>

namespace qindaqt::keyring::detail {
bool validKdf(KdfParameters p) noexcept {
    return p.memoryKiB >= 8192 && p.memoryKiB <= 262144
        && p.iterations >= 1 && p.iterations <= 8
        && p.lanes >= 1 && p.lanes <= 4 && p.memoryKiB >= 8 * p.lanes;
}
bool random(std::span<unsigned char> out) noexcept {
    return RAND_bytes(out.data(), static_cast<int>(out.size())) == 1;
}
bool derive(std::span<const unsigned char> password, std::span<const unsigned char> salt,
            KdfParameters p, SecureBuffer &key) {
    if (!validKdf(p) || salt.size() != 16 || password.size() > 4096) return false;
    std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)> kdf(EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr), EVP_KDF_free);
    if (!kdf) return false;
    std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)> ctx(EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free);
    unsigned int threads = 1, version = 0x13;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
            const_cast<unsigned char *>(password.data()), password.size()),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
            const_cast<unsigned char *>(salt.data()), salt.size()),
        OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_ARGON2_MEMCOST, &p.memoryKiB),
        OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_ITER, &p.iterations),
        OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_ARGON2_LANES, &p.lanes),
        OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_THREADS, &threads),
        OSSL_PARAM_construct_uint(OSSL_KDF_PARAM_ARGON2_VERSION, &version),
        OSSL_PARAM_construct_end()
    };
    key = SecureBuffer(32);
    if (!ctx || EVP_KDF_derive(ctx.get(), key.bytes().data(), key.size(), params) != 1) {
        key.clear();
        return false;
    }
    return true;
}
using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
bool encrypt(std::span<const unsigned char> key, std::span<const unsigned char> nonce,
             std::span<const unsigned char> aad, std::span<const unsigned char> plain,
             Bytes &cipher, std::array<unsigned char, 16> &tag) {
    CipherContext ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int written = 0, final = 0;
    cipher.resize(plain.size() + 16);
    if (!ctx || key.size() != 32 || nonce.size() != 12
        || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), nonce.data()) != 1
        || EVP_EncryptUpdate(ctx.get(), nullptr, &written, aad.data(), static_cast<int>(aad.size())) != 1
        || EVP_EncryptUpdate(ctx.get(), cipher.data(), &written, plain.data(), static_cast<int>(plain.size())) != 1
        || EVP_EncryptFinal_ex(ctx.get(), cipher.data() + written, &final) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        cipher.clear();
        return false;
    }
    cipher.resize(static_cast<std::size_t>(written + final));
    return true;
}
bool decrypt(std::span<const unsigned char> key, std::span<const unsigned char> nonce,
             std::span<const unsigned char> aad, std::span<const unsigned char> cipher,
             std::span<const unsigned char> tag, SecureBuffer &plain) {
    CipherContext ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    int written = 0, final = 0;
    plain = SecureBuffer(cipher.size() + 16);
    if (!ctx || key.size() != 32 || nonce.size() != 12 || tag.size() != 16
        || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, key.data(), nonce.data()) != 1
        || EVP_DecryptUpdate(ctx.get(), nullptr, &written, aad.data(), static_cast<int>(aad.size())) != 1
        || EVP_DecryptUpdate(ctx.get(), plain.bytes().data(), &written, cipher.data(), static_cast<int>(cipher.size())) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, 16,
                              const_cast<unsigned char *>(tag.data())) != 1
        || EVP_DecryptFinal_ex(ctx.get(), plain.bytes().data() + written, &final) != 1
        || static_cast<std::size_t>(written + final) != cipher.size()) {
        plain.clear();
        return false;
    }
    return true;
}
Digest attributeDigest(std::span<const unsigned char> key,
                       const std::string &name, const std::string &value) {
    // AGENT-GUARD: length framing prevents ("ab","c") == ("a","bc").
    Bytes input;
    for (const auto *part : {&name, &value}) {
        const auto size = static_cast<unsigned int>(part->size());
        for (int shift : {24, 16, 8, 0}) input.push_back(static_cast<unsigned char>(size >> shift));
        input.insert(input.end(), part->begin(), part->end());
    }
    Digest digest{};
    std::size_t length = 0;
    if (!EVP_Q_mac(nullptr, "HMAC", nullptr, "SHA256", nullptr, key.data(), key.size(),
                   input.data(), input.size(), digest.data(), digest.size(), &length)
        || length != digest.size()) throw std::runtime_error("Crypto unavailable");
    return digest;
}
}
