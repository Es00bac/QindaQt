// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QObject>
#include <QSocketNotifier>
#include <QString>

#include <functional>

namespace QindaQt::Apps::Terminal {

// AGENT-CONTRACT (ADR-0040, superseding ADR-0030): TerminalPtyBridge owns the
// application-side PTY that the terminal child runs on. The child opens the
// returned slave path by name, so it never inherits any bridge descriptor and
// its stdio keeps blocking semantics (the P1-1 review defect was O_NONBLOCK
// leaking through duplicated descriptors, and keyboard bytes written to a
// slave — which are output toward the master, never child input).
//
// Data directions, one writer per descriptor:
//  - keyboard/paste: writeInput() -> bridge master -> slave read = child
//    input (the only PTY direction that is input);
//  - child output and line-discipline echo: bridge master read -> injected
//    output channel (the adapter's private dup of the rendering widget's
//    teletype slave; slave write -> widget master read -> emulator).
// Buffers are bounded (64 KiB, drop-newest) and writes retry EINTR, parking
// on EAGAIN behind a write notifier.
//
// Read-side quiescence contract (P1: EIO hot loop): once pumpMasterToSink()
// observes a terminal read condition — EOF, Linux EIO after the last slave
// descriptor closes, or any hard read error — the read notifier is disabled
// for the rest of the generation. Linux keeps a hung-up master
// POLLHUP-readable forever, so leaving the notifier armed hot-loops the GUI
// thread. The master itself is NOT closed: closeChildChannel() is its only
// owner and the teardown's SIGHUP path, and exit truth stays with the
// session's ProcessMonitor reap (isChildOutputClosed() is an observation for
// tests/diagnostics, never an exit signal).
class TerminalPtyBridge final : public QObject {
  Q_OBJECT

public:
  struct OpenResult final {
    bool ok = false;
    QString diagnostic;
    QString slavePath; // The child opens this as its controlling TTY.
  };

  // Receives everything read from the bridge master (child output + echo).
  using OutputSink = std::function<void(const char *data, int length)>;

  explicit TerminalPtyBridge(OutputSink sink, QObject *parent = nullptr);
  ~TerminalPtyBridge() override;

  [[nodiscard]] OpenResult open();

  // Writes child-input bytes into the bridge master.
  void writeInput(const char *data, int length);

  // Applies the emulator's grid to the child tty; the kernel delivers
  // SIGWINCH to the child's foreground process group.
  void setChildWindowSize(int columns, int rows);

  // Closes the master. The kernel delivers SIGHUP to the child session, and
  // the bridge forwards nothing further. Safe to call repeatedly.
  void closeChildChannel();

  [[nodiscard]] bool isOpen() const { return m_masterFd >= 0; }

  // True once the master read side reported a terminal condition (EOF, EIO
  // after the last slave closed, or a hard error). Diagnostic/test
  // observation only: exit truth is the session's ProcessMonitor reap.
  [[nodiscard]] bool isChildOutputClosed() const {
    return m_childOutputClosed;
  }

private:
  void pumpMasterToSink();
  void flushInput();
  void appendBounded(QByteArray &buffer, const char *data, int length);

  OutputSink m_sink;
  QString m_slavePath;
  int m_masterFd = -1;
  bool m_childOutputClosed = false;
  QByteArray m_inputBuffer;
  QSocketNotifier *m_readNotifier = nullptr;
  QSocketNotifier *m_writeNotifier = nullptr;
};

} // namespace QindaQt::Apps::Terminal
