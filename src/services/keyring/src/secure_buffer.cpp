// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/secure_buffer.h>
#include <openssl/crypto.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdexcept>
#include <utility>
#include <limits>

namespace qindaqt::keyring {
SecureBuffer::SecureBuffer(std::size_t size) : ownerPid_(getpid()) {
    if (size == 0) return;
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    if (page == 0 || size > std::numeric_limits<std::size_t>::max() - page)
        throw std::runtime_error("Secure allocation unavailable");
    mapped_ = ((size + page - 1) / page) * page;
    void *mapping = mmap(nullptr, mapped_, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        mapped_ = 0;
        throw std::runtime_error("Secure allocation unavailable");
    }
    data_ = static_cast<unsigned char *>(mapping);
    if (mlock(data_, mapped_) != 0 || madvise(data_, mapped_, MADV_DONTDUMP) != 0
        || madvise(data_, mapped_, MADV_DONTFORK) != 0) {
        clear();
        throw std::runtime_error("Secure allocation unavailable");
    }
    size_ = size;
}
SecureBuffer::~SecureBuffer() { clear(); }
SecureBuffer::SecureBuffer(SecureBuffer &&other) noexcept {
    *this = std::move(other);
}
SecureBuffer &SecureBuffer::operator=(SecureBuffer &&other) noexcept {
    if (this != &other) {
        clear();
        ownerPid_ = std::exchange(other.ownerPid_, 0);
        data_ = std::exchange(other.data_, nullptr);
        size_ = std::exchange(other.size_, 0);
        mapped_ = std::exchange(other.mapped_, 0);
    }
    return *this;
}
std::size_t SecureBuffer::size() const noexcept {
    return ownerPid_ == getpid() ? size_ : 0;
}
std::span<unsigned char> SecureBuffer::bytes() noexcept {
    return ownerPid_ == getpid() ? std::span<unsigned char>{data_, size_} : std::span<unsigned char>{};
}
std::span<const unsigned char> SecureBuffer::bytes() const noexcept {
    return ownerPid_ == getpid() ? std::span<const unsigned char>{data_, size_} : std::span<const unsigned char>{};
}
void SecureBuffer::wipe() noexcept {
    if (data_ && ownerPid_ == getpid()) OPENSSL_cleanse(data_, mapped_);
}
void SecureBuffer::clear() noexcept {
    if (data_ && ownerPid_ == getpid()) {
        wipe();
        munlock(data_, mapped_);
        munmap(data_, mapped_);
    }
    data_ = nullptr;
    ownerPid_ = 0;
    size_ = mapped_ = 0;
}
}
