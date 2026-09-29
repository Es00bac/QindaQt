// SPDX-License-Identifier: GPL-3.0-or-later
#include "worker_process.h"
#include "process_protection.h"
#include <QFileInfo>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
namespace QindaQt::LockWorkerClient {
using namespace LockAuthentication;
WorkerProcess::WorkerProcess(QObject *parent)
    : QObject(parent), m_program(QStringLiteral(QINDAQT_LOCK_PAM_EXECUTABLE)) {
  m_deadline.setSingleShot(true);
  connect(&m_deadline, &QTimer::timeout, this, [this] { abort(Outcome::Unavailable); });
  connect(&m_process, &QProcess::started, this, [this] {
    if (m_childFd >= 0) { close(m_childFd); m_childFd = -1; }
  });
  connect(&m_process, &QProcess::finished, this, &WorkerProcess::finish);
  connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) { closeChannel(); complete(Outcome::Unavailable); }
  });
}
#if defined(QINDAQT_PRIVATE_WORKER_CLIENT_FIXTURE)
WorkerProcess::WorkerProcess(QString program, QString configuration, int deadlineMs)
    : WorkerProcess(nullptr) {
  m_program = std::move(program); m_arguments = {std::move(configuration)};
  m_timeoutMs = deadlineMs; m_fixture = true;
}
#endif
WorkerProcess::~WorkerProcess() {
  disconnect(&m_process, nullptr, this, nullptr); closeChannel();
  if (m_process.state() != QProcess::NotRunning) { m_process.kill(); m_process.waitForFinished(1000); }
}
bool WorkerProcess::start(AttemptToken token) {
  if (m_active || m_process.state() != QProcess::NotRunning || !token.epoch || !token.request ||
      !LockPlatform::restrictedPtracePolicy() || !LockPlatform::protectAuthority()) return false;
#if defined(QINDAQT_PRIVATE_WORKER_CLIENT_FIXTURE)
  const bool fixture = m_fixture;
#else
  constexpr bool fixture = false;
#endif
  if (!fixture && (!LockPlatform::rootManagedPath(m_program) || !QFileInfo(m_program).isExecutable())) return false;
  int pair[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, pair)) return false;
  m_fd = pair[0]; m_childFd = pair[1]; m_token = token; m_messages = 0;
  m_result.reset(); m_failure.reset(); m_active = true; m_waiting = false;
  m_reader = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read);
  m_writer = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Write);
  m_writer->setEnabled(false);
  connect(m_reader.get(), &QSocketNotifier::activated, this, [this] { drain(); });
  connect(m_writer.get(), &QSocketNotifier::activated, this, [this] { if (!flush()) abort(Outcome::Unavailable); });
  QProcessEnvironment environment;
  environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin:/bin"));
  environment.insert(QStringLiteral("LANG"), QStringLiteral("C.UTF-8"));
  m_process.setProcessEnvironment(environment);
  m_process.setProgram(m_program); m_process.setArguments(m_arguments);
  m_process.setWorkingDirectory(QStringLiteral("/"));
  m_process.setStandardOutputFile(QProcess::nullDevice());
  m_process.setStandardErrorFile(QProcess::nullDevice());
  QProcess::UnixProcessParameters parameters;
  parameters.flags = QProcess::UnixProcessFlag::CloseFileDescriptors | QProcess::UnixProcessFlag::ResetIds |
                     QProcess::UnixProcessFlag::DisableCoreDumps | QProcess::UnixProcessFlag::ResetSignalHandlers;
  parameters.lowestFileDescriptorToClose = 4; m_process.setUnixProcessParameters(parameters);
  const int childFd = m_childFd;
  m_process.setChildProcessModifier([childFd] {
    if (dup2(childFd, 3) < 0 || fcntl(3, F_SETFD, 0) < 0) _exit(127);
  });
  m_process.start(); m_deadline.start(m_timeoutMs);
  if (!send({WireKind::Begin, token, {}})) { abort(Outcome::Unavailable); return false; }
  return true;
}
bool WorkerProcess::respond(QString response) {
  if (!m_active || !m_waiting || m_result || m_failure) return false;
  auto bytes = response.toUtf8(); response.fill(QChar());
  std::string secret(bytes.constData(), static_cast<std::size_t>(bytes.size())); bytes.fill('\0');
  m_waiting = false;
  const bool ok = send({WireKind::Response, m_token, std::move(secret)});
  if (!ok) abort(Outcome::Unavailable);
  return ok;
}
bool WorkerProcess::send(WireFrame frame) {
  auto encoded = WorkerChannel::encode(frame);
  explicit_bzero(frame.payload.data(), frame.payload.size());
  if (!encoded || !m_outgoing.empty() || m_fd < 0) return false;
  m_outgoing = std::move(*encoded); m_written = 0;
  return flush();
}
bool WorkerProcess::flush() {
  if (m_fd < 0) return false;
  while (m_written < m_outgoing.size()) {
    const auto count = ::send(m_fd, m_outgoing.data() + m_written, m_outgoing.size() - m_written, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0 && errno == EAGAIN) { m_writer->setEnabled(true); return true; }
    if (count <= 0) return false;
    m_written += static_cast<std::size_t>(count);
  }
  explicit_bzero(m_outgoing.data(), m_outgoing.size()); m_outgoing.clear(); m_written = 0;
  m_writer->setEnabled(false); return true;
}
void WorkerProcess::drain() {
  if (m_fd < 0 || !m_active) return;
  std::array<char, WorkerChannel::maximumFrameBytes> bytes{};
  for (;;) {
    const auto count = recv(m_fd, bytes.data(), bytes.size(), MSG_DONTWAIT);
    if (count < 0 && errno == EINTR) continue;
    if (count < 0 && errno == EAGAIN) break;
    if (count <= 0) { m_reader->setEnabled(false); break; }
    m_incoming.append(bytes.data(), static_cast<std::size_t>(count));
    if (m_incoming.size() > 2 * WorkerChannel::maximumFrameBytes) { abort(Outcome::Unavailable); return; }
    while (m_incoming.size() >= WorkerChannel::headerBytes) {
      const std::size_t length = (static_cast<unsigned char>(m_incoming[6]) << 8) |
                                static_cast<unsigned char>(m_incoming[7]);
      if (length > maximumPromptBytes) { abort(Outcome::Unavailable); return; }
      const auto total = WorkerChannel::headerBytes + length;
      if (m_incoming.size() < total) break;
      auto frame = WorkerChannel::decode(std::string_view(m_incoming).substr(0, total));
      m_incoming.erase(0, total);
      if (!frame) { abort(Outcome::Unavailable); return; }
      consume(std::move(*frame));
      if (m_failure) return;
    }
  }
}
void WorkerProcess::consume(WireFrame frame) {
  if (frame.token != m_token || m_result || m_failure) { abort(Outcome::Unavailable); return; }
  if (frame.kind == WireKind::Result) {
    if (m_waiting || frame.payload.size() != 1 || frame.payload[0] < '0' || frame.payload[0] > '4') {
      abort(Outcome::Unavailable); return;
    }
    m_result = static_cast<Outcome>(frame.payload[0] - '0'); return;
  }
  if (m_waiting || ++m_messages > maximumMessages) { abort(Outcome::Unavailable); return; }
  MessageKind kind;
  switch (frame.kind) {
  case WireKind::Secret: kind = MessageKind::Secret; m_waiting = true; break;
  case WireKind::Visible: kind = MessageKind::Visible; m_waiting = true; break;
  case WireKind::Information: kind = MessageKind::Information; break;
  case WireKind::Error: kind = MessageKind::Error; break;
  default: abort(Outcome::Unavailable); return;
  }
  Q_EMIT prompt(kind, QString::fromUtf8(frame.payload.data(), static_cast<qsizetype>(frame.payload.size())));
}
void WorkerProcess::cancel() { abort(Outcome::Cancelled); }
void WorkerProcess::abort(Outcome reason) {
  if (!m_active) return;
  m_failure = reason; closeChannel();
  if (m_process.state() != QProcess::NotRunning) m_process.kill();
  else complete(reason);
}
void WorkerProcess::finish(int code, QProcess::ExitStatus status) {
  if (!m_active) return;
  drain();
  const auto result = m_failure.value_or(status == QProcess::NormalExit && code == 0 && m_result &&
                                       m_incoming.empty() ? *m_result : Outcome::Unavailable);
  closeChannel(); complete(result);
}
void WorkerProcess::closeChannel() {
  m_deadline.stop(); m_reader.reset(); m_writer.reset();
  if (m_fd >= 0) { close(m_fd); m_fd = -1; }
  if (m_childFd >= 0) { close(m_childFd); m_childFd = -1; }
  explicit_bzero(m_outgoing.data(), m_outgoing.size()); m_outgoing.clear(); m_incoming.clear(); m_written = 0;
}
void WorkerProcess::complete(Outcome outcome) {
  if (!m_active) return;
  m_active = false; m_waiting = false; Q_EMIT completed(m_token, outcome);
}
}
