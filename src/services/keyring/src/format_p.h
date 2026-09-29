// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "crypto_p.h"
#include <map>

namespace qindaqt::keyring::detail {
constexpr std::size_t MaxFile = 8 * 1024 * 1024;
constexpr std::size_t MaxPlain = 4 * 1024 * 1024;
constexpr std::size_t MaxSecret = 1024 * 1024;
constexpr std::size_t MaxItems = 1024;
constexpr std::size_t MaxAttributes = 32;
struct IndexItem {
    std::string id;
    std::vector<Digest> digests;
};
struct Envelope {
    KdfParameters parameters;
    std::array<unsigned char, 16> salt{};
    std::array<unsigned char, 12> nonce{};
    Digest searchKey{};
    std::array<unsigned char, 16> tag{};
    std::vector<IndexItem> index;
    Bytes aad;
    Bytes cipher;
};
bool validId(const std::string &id) noexcept;
bool validAttributes(const Attributes &attributes) noexcept;
bool validItem(const Item &item) noexcept;
std::vector<IndexItem> makeIndex(const std::map<std::string, Item> &items, const Digest &key);
bool sameIndex(const std::vector<IndexItem> &left, const std::vector<IndexItem> &right);
bool parseEnvelope(std::span<const unsigned char> file, Envelope &envelope);
Bytes makeAad(const Envelope &envelope, std::size_t cipherSize);
SecureBuffer encodeItems(const std::map<std::string, Item> &items);
bool decodeItems(std::span<const unsigned char> plain, std::map<std::string, Item> &items);
}
