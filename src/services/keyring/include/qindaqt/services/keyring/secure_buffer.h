// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <span>

namespace qindaqt::keyring {
// Move-only, thread-confined ownership of dedicated anonymous pages. Construction
// fails (throws) if mmap, mlock, or MADV_DONTDUMP/DONTFORK fails. Destruction/clear wipes
// the full mapping BEFORE munlock/unmap. No implicit copy or text conversion.
// AGENT-CONTRACT: daemon/PAM callers must also disable process core dumps and
// keep caller-owned password input out of ordinary temporaries (ADR-0292).
class SecureBuffer final {
public:
    explicit SecureBuffer(std::size_t size = 0);
    ~SecureBuffer();
    SecureBuffer(SecureBuffer &&other) noexcept;
    SecureBuffer &operator=(SecureBuffer &&other) noexcept;
    SecureBuffer(const SecureBuffer &) = delete;
    SecureBuffer &operator=(const SecureBuffer &) = delete;
    [[nodiscard]] std::span<unsigned char> bytes() noexcept;
    [[nodiscard]] std::span<const unsigned char> bytes() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    void wipe() noexcept; // Zero full mapping while retaining locked ownership.
    void clear() noexcept;
private:
    int ownerPid_ = 0; // Child inherits the object, never the secret mapping.
    unsigned char *data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t mapped_ = 0;
};
}
