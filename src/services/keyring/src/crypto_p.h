// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/collection_store.h>
#include <array>
#include <vector>

namespace qindaqt::keyring::detail {
using Bytes = std::vector<unsigned char>;
using Digest = std::array<unsigned char, 32>;
bool validKdf(KdfParameters parameters) noexcept;
bool random(std::span<unsigned char> output) noexcept;
bool derive(std::span<const unsigned char> password, std::span<const unsigned char> salt,
            KdfParameters parameters, SecureBuffer &key);
bool encrypt(std::span<const unsigned char> key, std::span<const unsigned char> nonce,
             std::span<const unsigned char> aad, std::span<const unsigned char> plain,
             Bytes &cipher, std::array<unsigned char, 16> &tag);
bool decrypt(std::span<const unsigned char> key, std::span<const unsigned char> nonce,
             std::span<const unsigned char> aad, std::span<const unsigned char> cipher,
             std::span<const unsigned char> tag, SecureBuffer &plain);
Digest attributeDigest(std::span<const unsigned char> key,
                       const std::string &name, const std::string &value);
}
