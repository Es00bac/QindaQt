// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/secure_buffer.h>
#include <functional>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace qindaqt::keyring {
using Attributes = std::map<std::string, std::string>;
enum class StoreError {
    None, InvalidInput, InvalidFormat, AuthenticationFailed, Locked,
    SecureMemoryUnavailable, CryptoUnavailable, IoError, DurabilityUnknown
};
struct KdfParameters {
    unsigned int memoryKiB = 65536;
    unsigned int iterations = 3;
    unsigned int lanes = 1;
};
struct ItemMetadata {
    std::string label;
    std::string contentType = "application/octet-stream";
    std::string creator; // Set from authenticated caller by the daemon, not storage.
    std::uint64_t created = 0;
    std::uint64_t modified = 0;
};
struct Item {
    std::string id;
    ItemMetadata metadata;
    Attributes attributes; // Non-secret metadata; never store passwords here.
    SecureBuffer secret;
};
struct SearchResult {
    std::vector<std::string> ids;
    // Loaded files cannot authenticate their index before unlock. Even verified
    // results disclose metadata and require unlock before reading any secret.
    bool authenticated = false;
};
// Synchronous, thread-confined, non-copyable collection. Owns all decrypted
// secrets and encryption key; lock()/failed unlock destroys them. Password spans
// are borrowed ONLY during create/unlock/rekey; caller owns wiping their input.
// Returns sanitized errors, never secret text. IDs are ASCII filename components.
// directory must be absolute, symlink-free, and owned by euid with mode 0700;
// files must be regular, singly-linked, euid-owned and 0600. No live paths chosen.
// commitBarrier is an optional cancellation gate just before atomic rename;
// false/throw leaves the original intact; the callback must not reenter storage. DurabilityUnknown means rename landed
// but parent fsync failed: callers must reload before reporting saved state.
// PK1 format v1 is bounded; future incompatible formats require explicit version.
class CollectionStore final {
public:
    CollectionStore(std::string directory, std::string collection,
                    std::function<bool()> commitBarrier = {});
    ~CollectionStore();
    CollectionStore(const CollectionStore &) = delete;
    CollectionStore &operator=(const CollectionStore &) = delete;
    // Initializes an empty in-memory collection; save refuses an existing file.
    StoreError create(std::span<const unsigned char> password, KdfParameters parameters = {});
    StoreError load();
    StoreError unlock(std::span<const unsigned char> password);
    void lock() noexcept;
    [[nodiscard]] bool locked() const noexcept;
    StoreError put(Item item); // Consumes the item even on failure.
    StoreError erase(const std::string &id);
    [[nodiscard]] const Item *item(const std::string &id) const noexcept;
    [[nodiscard]] const SecureBuffer *secret(const std::string &id) const noexcept;
    // Borrowed secret invalidated by put/erase/lock/load/unlock/rekey/destruction.
    [[nodiscard]] SearchResult search(const Attributes &query) const;
    StoreError save();
    // Consumes additions, rejects duplicate/existing IDs and stages the complete
    // current collection without mutating it. One durable save publishes all.
    // Pre-rename failures preserve memory/disk and borrowed pointers. Success
    // invalidates pointers; DurabilityUnknown reloads locked and publishes no
    // decrypted candidate. Empty batches are invalid; no domain/import policy.
    StoreError insertBatchAndSave(std::vector<Item> additions);
    StoreError rekey(std::span<const unsigned char> password, KdfParameters parameters = {});
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
