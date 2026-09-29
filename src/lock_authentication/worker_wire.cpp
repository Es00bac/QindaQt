// SPDX-License-Identifier: GPL-3.0-or-later
#include "worker_wire.h"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
namespace QindaQt::LockAuthentication {
namespace {
void put(std::string &bytes, std::size_t offset, std::uint64_t value, std::size_t width) {
  for (std::size_t i = 0; i < width; ++i) {
    bytes[offset + width - i - 1] = static_cast<char>(value & 255U);
    value >>= 8;
  }
}
std::uint64_t get(std::string_view bytes, std::size_t offset, std::size_t width) {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < width; ++i) {
    value = (value << 8) | static_cast<unsigned char>(bytes[offset + i]);
  }
  return value;
}
bool validHeader(std::string_view bytes) {
  return bytes.size() >= WorkerChannel::headerBytes && bytes.substr(0, 3) == "QLA" &&
         bytes[3] == 1 && static_cast<unsigned char>(bytes[4]) >= 1 &&
         static_cast<unsigned char>(bytes[4]) <= 8 && bytes[5] == 0 &&
         get(bytes, 6, 2) <= maximumPromptBytes && get(bytes, 8, 8) && get(bytes, 16, 8);
}
}
WorkerChannel::WorkerChannel(int ownedFd, std::chrono::milliseconds timeout)
    : m_fd(ownedFd), m_deadline(std::chrono::steady_clock::now() + timeout) {
  int type = 0;
  socklen_t size = sizeof(type);
  m_failed = m_fd < 0 || timeout.count() <= 0 ||
             getsockopt(m_fd, SOL_SOCKET, SO_TYPE, &type, &size) || type != SOCK_STREAM;
}
WorkerChannel::~WorkerChannel() {
  if (m_fd >= 0) close(m_fd);
}
std::optional<std::string> WorkerChannel::encode(const WireFrame &frame) {
  const auto kind = static_cast<unsigned char>(frame.kind);
  if (kind < 1 || kind > 8 || !frame.token.epoch || !frame.token.request ||
      frame.payload.size() > maximumPromptBytes || frame.payload.find('\0') != std::string::npos) {
    return std::nullopt;
  }
  std::string bytes(headerBytes, '\0');
  bytes.replace(0, 3, "QLA"); bytes[3] = 1; bytes[4] = static_cast<char>(kind);
  put(bytes, 6, frame.payload.size(), 2);
  put(bytes, 8, frame.token.epoch, 8); put(bytes, 16, frame.token.request, 8);
  bytes += frame.payload;
  return bytes;
}
std::optional<WireFrame> WorkerChannel::decode(std::string_view bytes) {
  if (!validHeader(bytes) || bytes.size() != headerBytes + get(bytes, 6, 2) ||
      bytes.substr(headerBytes).find('\0') != std::string_view::npos) return std::nullopt;
  return WireFrame{static_cast<WireKind>(bytes[4]),
                   {get(bytes, 8, 8), get(bytes, 16, 8)}, std::string(bytes.substr(headerBytes))};
}
bool WorkerChannel::transfer(char *bytes, std::size_t size, bool writing) {
  while (size && !m_failed) {
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_deadline - std::chrono::steady_clock::now()).count();
    if (remaining <= 0) { m_failed = true; break; }
    pollfd descriptor{m_fd, static_cast<short>(writing ? POLLOUT : POLLIN), 0};
    const int ready = poll(&descriptor, 1, static_cast<int>(std::min<long long>(remaining, INT_MAX)));
    if (ready < 0 && errno == EINTR) continue;
    if (ready <= 0 || !(descriptor.revents & descriptor.events)) { m_failed = true; break; }
    const auto transferred = writing ? ::send(m_fd, bytes, size, MSG_NOSIGNAL | MSG_DONTWAIT)
                                     : recv(m_fd, bytes, size, MSG_DONTWAIT);
    if (transferred < 0 && (errno == EINTR || errno == EAGAIN)) continue;
    if (transferred <= 0) { m_failed = true; break; }
    bytes += transferred; size -= static_cast<std::size_t>(transferred);
  }
  return !m_failed;
}
bool WorkerChannel::send(const WireFrame &frame) {
  auto bytes = encode(frame);
  if (!bytes) { m_failed = true; return false; }
  const bool ok = transfer(bytes->data(), bytes->size(), true);
  explicit_bzero(bytes->data(), bytes->size());
  return ok;
}
std::optional<WireFrame> WorkerChannel::receive() {
  std::string bytes(headerBytes, '\0');
  if (!transfer(bytes.data(), bytes.size(), false) || !validHeader(bytes)) {
    m_failed = true; return std::nullopt;
  }
  const std::size_t size = static_cast<std::size_t>(get(bytes, 6, 2));
  bytes.resize(headerBytes + size);
  if (!transfer(bytes.data() + headerBytes, size, false)) return std::nullopt;
  auto frame = decode(bytes);
  explicit_bzero(bytes.data(), bytes.size());
  if (!frame) m_failed = true;
  return frame;
}
bool WorkerChannel::pending() const {
  pollfd descriptor{m_fd, POLLIN, 0};
  return m_failed || std::chrono::steady_clock::now() >= m_deadline ||
         (poll(&descriptor, 1, 0) > 0 && descriptor.revents);
}
} // namespace QindaQt::LockAuthentication
