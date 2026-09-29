// SPDX-License-Identifier: GPL-3.0-or-later
#include "format_p.h"
#include <openssl/crypto.h>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace qindaqt::keyring::detail {
namespace {
struct FormatError : std::runtime_error { FormatError() : std::runtime_error("Invalid format") {} };
constexpr unsigned char Magic[] = {'Q','K','E','Y','R','0','0','1'};
class Reader {
public:
    explicit Reader(std::span<const unsigned char> bytes) : bytes_(bytes) {}
    std::span<const unsigned char> take(std::size_t size) {
        if (size > bytes_.size()) throw FormatError{};
        const auto result = bytes_.first(size);
        bytes_ = bytes_.subspan(size);
        return result;
    }
    unsigned int number() {
        const auto b = take(4);
        return (static_cast<unsigned int>(b[0]) << 24)
            | (static_cast<unsigned int>(b[1]) << 16)
            | (static_cast<unsigned int>(b[2]) << 8) | b[3];
    }
    std::uint64_t timestamp() {
        const auto high = number();
        return (static_cast<std::uint64_t>(high) << 32) | number();
    }
    std::string string(std::size_t maximum) {
        const auto size = number();
        if (size > maximum) throw FormatError{};
        const auto b = take(size);
        return {reinterpret_cast<const char *>(b.data()), b.size()};
    }
    bool done() const { return bytes_.empty(); }
private:
    std::span<const unsigned char> bytes_;
};
class Writer {
public:
    explicit Writer(std::span<unsigned char> bytes) : bytes_(bytes) {}
    void raw(std::span<const unsigned char> source) {
        if (source.size() > bytes_.size()) throw FormatError{};
        if (!source.empty()) std::memcpy(bytes_.data(), source.data(), source.size());
        bytes_ = bytes_.subspan(source.size());
    }
    void number(unsigned int value) {
        const unsigned char b[] = {static_cast<unsigned char>(value >> 24),
            static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 8),
            static_cast<unsigned char>(value)};
        raw(b);
    }
    void timestamp(std::uint64_t value) {
        number(static_cast<unsigned int>(value >> 32));
        number(static_cast<unsigned int>(value));
    }
    void string(const std::string &value) {
        number(static_cast<unsigned int>(value.size()));
        raw({reinterpret_cast<const unsigned char *>(value.data()), value.size()});
    }
private:
    std::span<unsigned char> bytes_;
};
void appendNumber(Bytes &out, unsigned int value) {
    for (int shift : {24,16,8,0}) out.push_back(static_cast<unsigned char>(value >> shift));
}
void appendString(Bytes &out, const std::string &value) {
    appendNumber(out, static_cast<unsigned int>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}
template<std::size_t N> void readArray(Reader &r, std::array<unsigned char, N> &out) {
    const auto b = r.take(N);
    std::copy(b.begin(), b.end(), out.begin());
}
bool digestLess(const Digest &a, const Digest &b) { return a < b; }
}
bool validId(const std::string &id) noexcept {
    return !id.empty() && id.size() <= 64 && id != "." && id != ".."
        && std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        });
}
bool validAttributes(const Attributes &attributes) noexcept {
    return attributes.size() <= MaxAttributes
        && std::all_of(attributes.begin(), attributes.end(), [](const auto &entry) {
            return !entry.first.empty() && entry.first.size() <= 1024 && entry.second.size() <= 1024;
        });
}
bool validItem(const Item &item) noexcept {
    return item.metadata.label.size() <= 1024 && !item.metadata.contentType.empty()
        && item.metadata.contentType.size() <= 128 && item.metadata.creator.size() <= 1024
        && validId(item.id) && validAttributes(item.attributes) && item.secret.size() <= MaxSecret;
}
std::vector<IndexItem> makeIndex(const std::map<std::string, Item> &items, const Digest &key) {
    std::vector<IndexItem> index;
    for (const auto &[id, item] : items) {
        IndexItem entry{id, {}};
        for (const auto &[name, value] : item.attributes)
            entry.digests.push_back(attributeDigest(key, name, value));
        std::sort(entry.digests.begin(), entry.digests.end(), digestLess);
        index.push_back(std::move(entry));
    }
    return index;
}
bool sameIndex(const std::vector<IndexItem> &a, const std::vector<IndexItem> &b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].digests.size() != b[i].digests.size()) return false;
        for (std::size_t j = 0; j < a[i].digests.size(); ++j)
            if (CRYPTO_memcmp(a[i].digests[j].data(), b[i].digests[j].data(), 32) != 0) return false;
    }
    return true;
}
Bytes makeAad(const Envelope &e, std::size_t cipherSize) {
    Bytes index;
    appendNumber(index, static_cast<unsigned int>(e.index.size()));
    for (const auto &item : e.index) {
        appendString(index, item.id);
        appendNumber(index, static_cast<unsigned int>(item.digests.size()));
        for (const auto &digest : item.digests) index.insert(index.end(), digest.begin(), digest.end());
    }
    Bytes aad(std::begin(Magic), std::end(Magic));
    appendNumber(aad, e.parameters.memoryKiB);
    appendNumber(aad, e.parameters.iterations);
    appendNumber(aad, e.parameters.lanes);
    aad.insert(aad.end(), e.salt.begin(), e.salt.end());
    aad.insert(aad.end(), e.nonce.begin(), e.nonce.end());
    aad.insert(aad.end(), e.searchKey.begin(), e.searchKey.end());
    appendNumber(aad, static_cast<unsigned int>(index.size()));
    appendNumber(aad, static_cast<unsigned int>(cipherSize));
    aad.insert(aad.end(), index.begin(), index.end());
    return aad;
}
bool parseEnvelope(std::span<const unsigned char> file, Envelope &out) {
    if (file.size() > MaxFile) return false;
    try {
        Envelope e;
        Reader r(file);
        const auto magic = r.take(8);
        if (!std::equal(magic.begin(), magic.end(), std::begin(Magic))) return false;
        e.parameters = {r.number(), r.number(), r.number()};
        if (!validKdf(e.parameters)) return false;
        readArray(r, e.salt); readArray(r, e.nonce); readArray(r, e.searchKey);
        const auto indexSize = r.number(), cipherSize = r.number();
        // AGENT-GUARD: bound every hostile length and KDF parameter BEFORE
        // deriving a key, allocating locked plaintext, or iterating item counts.
        if (indexSize > 4 + MaxItems * (4 + 64 + 4 + MaxAttributes * 32)
            || cipherSize < 4 || cipherSize > MaxPlain
            || file.size() != 88 + indexSize + cipherSize + 16) return false;
        Reader index(r.take(indexSize));
        const auto count = index.number();
        if (count > MaxItems) return false;
        std::string previous;
        for (unsigned int i = 0; i < count; ++i) {
            IndexItem item{index.string(64), {}};
            if (!validId(item.id) || (!previous.empty() && item.id <= previous)) return false;
            previous = item.id;
            const auto attrs = index.number();
            if (attrs > MaxAttributes) return false;
            for (unsigned int j = 0; j < attrs; ++j) {
                Digest digest{};
                readArray(index, digest);
                if (!item.digests.empty() && digest <= item.digests.back()) return false;
                item.digests.push_back(digest);
            }
            e.index.push_back(std::move(item));
        }
        if (!index.done()) return false;
        e.aad.assign(file.begin(), file.begin() + 88 + indexSize);
        const auto cipher = r.take(cipherSize);
        e.cipher.assign(cipher.begin(), cipher.end());
        readArray(r, e.tag);
        if (!r.done()) return false;
        out = std::move(e);
        return true;
    } catch (const FormatError &) { return false; }
}
SecureBuffer encodeItems(const std::map<std::string, Item> &items) {
    std::size_t total = 4;
    for (const auto &[id, item] : items) {
        if (id != item.id || !validItem(item)) throw std::runtime_error("Invalid input");
        total += 4 + id.size() + 4 + 4 + item.secret.size()
            + 28 + item.metadata.label.size() + item.metadata.contentType.size() + item.metadata.creator.size();
        for (const auto &[name, value] : item.attributes) total += 8 + name.size() + value.size();
        if (total > MaxPlain) throw std::runtime_error("Invalid input");
    }
    SecureBuffer plain(total);
    Writer w(plain.bytes());
    w.number(static_cast<unsigned int>(items.size()));
    for (const auto &[id, item] : items) {
        w.string(id);
        w.string(item.metadata.label); w.string(item.metadata.contentType); w.string(item.metadata.creator);
        w.timestamp(item.metadata.created); w.timestamp(item.metadata.modified);
        w.number(static_cast<unsigned int>(item.attributes.size()));
        for (const auto &[name, value] : item.attributes) { w.string(name); w.string(value); }
        w.number(static_cast<unsigned int>(item.secret.size()));
        w.raw(item.secret.bytes());
    }
    return plain;
}
bool decodeItems(std::span<const unsigned char> plain, std::map<std::string, Item> &out) {
    try {
        Reader r(plain);
        const auto count = r.number();
        if (count > MaxItems) return false;
        std::map<std::string, Item> items;
        for (unsigned int i = 0; i < count; ++i) {
            Item item;
            item.id = r.string(64);
            if (!validId(item.id) || items.contains(item.id)) return false;
            item.metadata.label = r.string(1024);
            item.metadata.contentType = r.string(128);
            item.metadata.creator = r.string(1024);
            item.metadata.created = r.timestamp(); item.metadata.modified = r.timestamp();
            if (item.metadata.contentType.empty()) return false;
            const auto countAttrs = r.number();
            if (countAttrs > MaxAttributes) return false;
            for (unsigned int j = 0; j < countAttrs; ++j) {
                auto name = r.string(1024), value = r.string(1024);
                if (name.empty() || !item.attributes.emplace(std::move(name), std::move(value)).second) return false;
            }
            const auto size = r.number();
            if (size > MaxSecret) return false;
            const auto secret = r.take(size);
            item.secret = SecureBuffer(size);
            std::copy(secret.begin(), secret.end(), item.secret.bytes().begin());
            auto id = item.id;
            items.emplace(std::move(id), std::move(item));
        }
        if (!r.done()) return false;
        out = std::move(items);
        return true;
    } catch (const FormatError &) { return false; }
}
}
