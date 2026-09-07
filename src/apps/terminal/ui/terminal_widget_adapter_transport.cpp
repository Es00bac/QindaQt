// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_widget_adapter.h"

#include "session/pty_bridge.h"

#include <qtermwidget.h>

#include <QSocketNotifier>

#include <cerrno>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace QindaQt::Apps::Terminal {
namespace {

// Buffers toward the renderer are bounded and drop newest under sustained
// backpressure, keeping child output from blocking or growing the GUI process.
constexpr qsizetype kMaxWidgetOutputBufferBytes = 64 * 1024;

} // namespace

bool TerminalWidgetAdapter::initializeChannels() {
  // AGENT-GUARD (production Wayland): TerminalSession must publish the widget
  // synchronously before this starts the teletype. qtermwidget 2.4 does not
  // recover glyph painting after a live parentless display is reparented.
  m_widget->startTerminalTeletype();

  const int widgetSlaveFd = m_widget->getPtySlaveFd();
  if (widgetSlaveFd < 0) {
    m_transportDiagnostic = QStringLiteral("Rendering teletype is unavailable");
    return false;
  }
  m_widgetSlaveFd = ::dup(widgetSlaveFd);
  if (m_widgetSlaveFd < 0) {
    m_transportDiagnostic =
        QStringLiteral("Cannot duplicate the rendering teletype");
    return false;
  }
  ::fcntl(m_widgetSlaveFd, F_SETFD, FD_CLOEXEC);
  const int flags = ::fcntl(m_widgetSlaveFd, F_GETFL, 0);
  if (flags >= 0) {
    ::fcntl(m_widgetSlaveFd, F_SETFL, flags | O_NONBLOCK);
  }
  makeWidgetTransportByteTransparent();
  primeWidgetTransport();
  if (!m_transportDiagnostic.isEmpty()) {
    return false;
  }
  m_widgetOutputNotifier =
      new QSocketNotifier(m_widgetSlaveFd, QSocketNotifier::Write, this);
  m_widgetOutputNotifier->setEnabled(false);
  connect(m_widgetOutputNotifier, &QSocketNotifier::activated, this,
          [this] { flushChildOutputToWidget(); });

  m_bridge = new TerminalPtyBridge(
      [this](const char *data, int length) {
        forwardChildOutput(data, length);
      },
      this);
  const auto opened = m_bridge->open();
  m_bridgeDiagnostic = opened.diagnostic;
  m_slavePath = opened.slavePath;
  return opened.ok;
}

void TerminalWidgetAdapter::makeWidgetTransportByteTransparent() {
  // AGENT-CONTRACT (ADR-0040): the child PTY already applied its line
  // discipline. The rendering-only PTY must not transform those bytes again.
  termios settings{};
  if (::tcgetattr(m_widgetSlaveFd, &settings) != 0) {
    m_transportDiagnostic =
        QStringLiteral("Cannot read the rendering teletype settings");
    return;
  }
  settings.c_oflag &= static_cast<tcflag_t>(~OPOST);
  if (::tcsetattr(m_widgetSlaveFd, TCSANOW, &settings) != 0) {
    m_transportDiagnostic =
        QStringLiteral("Cannot clear rendering teletype output processing");
    return;
  }
  termios verified{};
  if (::tcgetattr(m_widgetSlaveFd, &verified) != 0 ||
      (verified.c_oflag & OPOST) != 0) {
    m_transportDiagnostic = QStringLiteral(
        "Rendering teletype output processing could not be disabled");
  }
}

void TerminalWidgetAdapter::primeWidgetTransport() {
  // AGENT-GUARD: qtermwidget 2.4 treats a zero-byte first KPty activation as
  // permanent EOF. Ensure its first activation consumes an invisible value.
  constexpr char reset[] = "\x1b[0m";
  size_t offset = 0;
  while (offset < sizeof(reset) - 1) {
    const ssize_t written = ::write(
        m_widgetSlaveFd, reset + offset, sizeof(reset) - 1 - offset);
    if (written > 0) {
      offset += static_cast<size_t>(written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    m_transportDiagnostic =
        QStringLiteral("Cannot prime the rendering teletype");
    return;
  }
}

void TerminalWidgetAdapter::closeChildChannel() {
  if (m_bridge != nullptr) {
    m_bridge->closeChildChannel();
  }
  if (m_widgetOutputNotifier != nullptr) {
    m_widgetOutputNotifier->setEnabled(false);
    m_widgetOutputNotifier->deleteLater();
    m_widgetOutputNotifier = nullptr;
  }
  m_widgetOutputBuffer.clear();
  if (m_widgetSlaveFd >= 0) {
    ::close(m_widgetSlaveFd);
    m_widgetSlaveFd = -1;
  }
}

void TerminalWidgetAdapter::forwardChildOutput(const char *data, int length) {
  if (data == nullptr || length <= 0 || m_widgetSlaveFd < 0 ||
      m_widgetOutputBuffer.size() >= kMaxWidgetOutputBufferBytes) {
    return;
  }
  const qsizetype room =
      kMaxWidgetOutputBufferBytes - m_widgetOutputBuffer.size();
  m_widgetOutputBuffer.append(data, qMin<qsizetype>(length, room));
  flushChildOutputToWidget();
}

void TerminalWidgetAdapter::flushChildOutputToWidget() {
  while (!m_widgetOutputBuffer.isEmpty() && m_widgetSlaveFd >= 0) {
    const ssize_t written =
        ::write(m_widgetSlaveFd, m_widgetOutputBuffer.constData(),
                static_cast<size_t>(m_widgetOutputBuffer.size()));
    if (written > 0) {
      m_widgetOutputBuffer.remove(0, written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if (m_widgetOutputNotifier != nullptr) {
        m_widgetOutputNotifier->setEnabled(true);
      }
      return;
    }
    m_widgetOutputBuffer.clear();
    return;
  }
  if (m_widgetOutputNotifier != nullptr) {
    m_widgetOutputNotifier->setEnabled(false);
  }
}

} // namespace QindaQt::Apps::Terminal
