// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/collection_store.h>
#include "atomic_file_p.h"
#include "format_p.h"
#include <openssl/crypto.h>
#include <algorithm>
#include <stdexcept>

namespace qindaqt::keyring {
using namespace detail;
struct CollectionStore::State {
    std::string directory, file;
    std::function<bool()> barrier;
    Envelope envelope;
    SecureBuffer key;
    std::map<std::string, Item> items;
    bool initialized = false;
    bool authenticated = false;
    bool newCollection = false;
};
CollectionStore::CollectionStore(std::string directory, std::string collection,
                                 std::function<bool()> barrier)
    : state_(std::make_unique<State>()) {
    state_->directory = std::move(directory);
    if (validId(collection) && collection.size() <= 60) state_->file = collection + ".qkr";
    state_->barrier = std::move(barrier);
}
CollectionStore::~CollectionStore() = default;
void CollectionStore::lock() noexcept {
    state_->items.clear();
    state_->key.clear();
}
bool CollectionStore::locked() const noexcept { return state_->key.size() != 32; }
StoreError CollectionStore::create(std::span<const unsigned char> password, KdfParameters parameters) {
    lock();
    state_->initialized = state_->authenticated = false;
    if (state_->file.empty() || password.size() > 4096 || !validKdf(parameters)) return StoreError::InvalidInput;
    try {
        Envelope e;
        e.parameters = parameters;
        if (!random(e.salt) || !random(e.searchKey)) return StoreError::CryptoUnavailable;
        SecureBuffer key;
        if (!derive(password, e.salt, parameters, key)) return StoreError::CryptoUnavailable;
        state_->envelope = std::move(e);
        state_->key = std::move(key);
        state_->initialized = state_->authenticated = state_->newCollection = true;
        return StoreError::None;
    } catch (const std::runtime_error &) { return StoreError::SecureMemoryUnavailable; }
}
StoreError CollectionStore::load() {
    lock();
    state_->initialized = state_->authenticated = state_->newCollection = false;
    state_->envelope = {};
    Bytes file;
    const auto error = readFile(state_->directory, state_->file, file);
    if (error != StoreError::None) return error;
    if (!parseEnvelope(file, state_->envelope)) return StoreError::InvalidFormat;
    state_->initialized = true;
    return StoreError::None;
}
StoreError CollectionStore::unlock(std::span<const unsigned char> password) {
    lock();
    state_->authenticated = false;
    if (!state_->initialized || state_->envelope.cipher.empty()) return StoreError::InvalidFormat;
    if (password.size() > 4096) return StoreError::InvalidInput;
    try {
        SecureBuffer key, plain;
        const auto &e = state_->envelope;
        if (!derive(password, e.salt, e.parameters, key)) return StoreError::CryptoUnavailable;
        if (!decrypt(key.bytes(), e.nonce, e.aad, e.cipher, e.tag, plain)) return StoreError::AuthenticationFailed;
        std::map<std::string, Item> items;
        if (!decodeItems(plain.bytes().first(e.cipher.size()), items)
            || !sameIndex(e.index, makeIndex(items, e.searchKey))) return StoreError::InvalidFormat;
        state_->items = std::move(items);
        state_->key = std::move(key);
        state_->authenticated = true;
        return StoreError::None;
    } catch (const std::runtime_error &) { return StoreError::SecureMemoryUnavailable; }
}
StoreError CollectionStore::put(Item item) {
    if (locked()) return StoreError::Locked;
    if (!validItem(item) || (!state_->items.contains(item.id) && state_->items.size() >= MaxItems))
        return StoreError::InvalidInput;
    // Bound aggregate plaintext before accepting an item; avoids a collection
    // whose persistence can only fail after expensive work or large allocation.
    std::size_t total = 4;
    const auto sizeOf = [](const Item &value) {
        std::size_t size = 40 + value.id.size() + value.secret.size()
            + value.metadata.label.size() + value.metadata.contentType.size() + value.metadata.creator.size();
        for (const auto &[name, valueText] : value.attributes) size += 8 + name.size() + valueText.size();
        return size;
    };
    for (const auto &[id, existing] : state_->items) if (id != item.id) total += sizeOf(existing);
    if (total + sizeOf(item) > MaxPlain) return StoreError::InvalidInput;
    try {
        auto id = item.id;
        state_->items.insert_or_assign(std::move(id), std::move(item));
        return StoreError::None;
    } catch (const std::runtime_error &) { lock(); return StoreError::CryptoUnavailable; }
}
StoreError CollectionStore::erase(const std::string &id) {
    if (locked()) return StoreError::Locked;
    if (!state_->items.erase(id)) return StoreError::InvalidInput;
    try {
        return StoreError::None;
    } catch (const std::runtime_error &) { lock(); return StoreError::CryptoUnavailable; }
}
const Item *CollectionStore::item(const std::string &id) const noexcept {
    if (locked()) return nullptr;
    const auto found = state_->items.find(id);
    return found == state_->items.end() ? nullptr : &found->second;
}
const SecureBuffer *CollectionStore::secret(const std::string &id) const noexcept {
    if (locked()) return nullptr;
    const auto found = state_->items.find(id);
    return found == state_->items.end() ? nullptr : &found->second.secret;
}
SearchResult CollectionStore::search(const Attributes &query) const {
    SearchResult result{{}, state_->authenticated};
    if (!state_->initialized || !validAttributes(query)) return result;
    try {
        std::vector<Digest> digests;
        for (const auto &[name, value] : query)
            digests.push_back(attributeDigest(state_->envelope.searchKey, name, value));
        const auto index = locked() ? state_->envelope.index : makeIndex(state_->items, state_->envelope.searchKey);
        for (const auto &entry : index) {
            bool matches = true;
            for (const auto &digest : digests) {
                if (!std::any_of(entry.digests.begin(), entry.digests.end(), [&](const auto &stored) {
                    return CRYPTO_memcmp(digest.data(), stored.data(), digest.size()) == 0;
                })) { matches = false; break; }
            }
            if (matches) result.ids.push_back(entry.id);
        }
    } catch (const std::runtime_error &) { return {{}, false}; }
    return result;
}
StoreError CollectionStore::save() {
    if (locked()) return StoreError::Locked;
    try {
        auto plain = encodeItems(state_->items);
        Envelope candidate = state_->envelope;
        candidate.index = makeIndex(state_->items, candidate.searchKey);
        if (!random(candidate.nonce)) return StoreError::CryptoUnavailable;
        candidate.aad = makeAad(candidate, plain.size());
        if (!encrypt(state_->key.bytes(), candidate.nonce, candidate.aad,
                     plain.bytes(), candidate.cipher, candidate.tag)) return StoreError::CryptoUnavailable;
        Bytes file = candidate.aad;
        file.insert(file.end(), candidate.cipher.begin(), candidate.cipher.end());
        file.insert(file.end(), candidate.tag.begin(), candidate.tag.end());
        const auto error = replaceFile(state_->directory, state_->file, file,
                                       state_->barrier, state_->newCollection);
        if (error == StoreError::None || error == StoreError::DurabilityUnknown) {
            state_->envelope = std::move(candidate);
            state_->newCollection = false;
        }
        return error;
    } catch (const std::runtime_error &) { return StoreError::SecureMemoryUnavailable; }
}
StoreError CollectionStore::rekey(std::span<const unsigned char> password, KdfParameters parameters) {
    if (locked()) return StoreError::Locked;
    if (password.size() > 4096 || !validKdf(parameters)) return StoreError::InvalidInput;
    try {
        auto original = state_->envelope;
        SecureBuffer oldKey = std::move(state_->key);
        std::array<unsigned char, 16> salt{};
        SecureBuffer newKey;
        if (!random(salt) || !derive(password, salt, parameters, newKey)) {
            state_->key = std::move(oldKey);
            return StoreError::CryptoUnavailable;
        }
        state_->key = std::move(newKey);
        state_->envelope.salt = salt;
        state_->envelope.parameters = parameters;
        const auto result = save();
        if (result != StoreError::None && result != StoreError::DurabilityUnknown) {
            state_->envelope = std::move(original);
            state_->key = std::move(oldKey);
        }
        return result;
    } catch (const std::runtime_error &) {
        // Any unexpected failure while staging rekey must never retain a
        // mismatched key/envelope pair. Persistent bytes remain reloadable.
        lock();
        return StoreError::SecureMemoryUnavailable;
    }
}
}
