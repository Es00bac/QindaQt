// SPDX-License-Identifier: GPL-3.0-or-later
#include "session/pty_bridge.h"

#include <QTest>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <vector>

// AGENT-CONTRACT: These rows prove the P1-1 architecture with a real
// kernel PTY, no display and no qtermwidget: master writes must arrive on
// the slave as child INPUT, slave writes must arrive at the sink as
// output/echo (the rendering direction), and TIOCSWINSZ must reach the
// slave side. This is registered evidence, not a GUI/live-lane claim.
using QindaQt::Apps::Terminal::TerminalPtyBridge;

namespace {

constexpr int kSinkWaitMs = 3000;

// Opens the slave path exactly like the child does and switches it to raw
// mode so reads are deterministic (no canonical buffering, no echo unless
// the row enables it).
int openRawSlave(const QString &path) {
  const int slave = ::open(path.toUtf8().constData(), O_RDWR | O_NOCTTY);
  if (slave < 0) {
    return -1;
  }
  termios settings{};
  if (::tcgetattr(slave, &settings) == 0) {
    cfmakeraw(&settings);
    ::tcsetattr(slave, TCSANOW, &settings);
  }
  return slave;
}

} // namespace

class TerminalPtyBridgeTest final : public QObject {
  Q_OBJECT

private slots:
  void openProvidesASlavePath();
  void inputReachesTheSlaveAsChildInput();
  void outputAndEchoFlowFromMasterToSink();
  void childWindowSizeIsApplied();
  void closeStopsForwarding();
};

void TerminalPtyBridgeTest::openProvidesASlavePath() {
  TerminalPtyBridge bridge([](const char *, int) {}, this);
  const auto opened = bridge.open();
  QVERIFY(opened.ok);
  QVERIFY(!opened.slavePath.isEmpty());
  QVERIFY(opened.slavePath.startsWith(QLatin1String("/dev/pts/")));
  QVERIFY(bridge.isOpen());
  bridge.closeChildChannel();
  QVERIFY(!bridge.isOpen());
}

void TerminalPtyBridgeTest::inputReachesTheSlaveAsChildInput() {
  TerminalPtyBridge bridge([](const char *, int) {}, this);
  const auto opened = bridge.open();
  QVERIFY(opened.ok);
  const int slave = openRawSlave(opened.slavePath);
  QVERIFY(slave >= 0);

  // Master write -> slave read is the only PTY direction that is child
  // input; this is the exact semantics the P1-1 defect got backwards.
  const QByteArray typed = QByteArrayLiteral("echo hi\r");
  bridge.writeInput(typed.constData(),
                    static_cast<int>(typed.size()));

  QByteArray received;
  QVERIFY(QTest::qWaitFor([&received, typed, slave] {
    char chunk[256];
    const ssize_t chunkSize = ::read(slave, chunk, sizeof(chunk));
    if (chunkSize > 0) {
      received.append(chunk, static_cast<qsizetype>(chunkSize));
    }
    return received.contains(typed);
  }, kSinkWaitMs));
  QVERIFY(received.contains(typed));
  ::close(slave);
  bridge.closeChildChannel();
}

void TerminalPtyBridgeTest::outputAndEchoFlowFromMasterToSink() {
  std::vector<QByteArray> sink;
  TerminalPtyBridge bridge(
      [&sink](const char *data, int length) {
        sink.emplace_back(data, static_cast<int>(length));
      },
      this);
  const auto opened = bridge.open();
  QVERIFY(opened.ok);
  const int slave = openRawSlave(opened.slavePath);
  QVERIFY(slave >= 0);

  // Child output written to the slave must arrive at the sink (which feeds
  // the rendering widget's teletype slave in production).
  const QByteArray output = QByteArrayLiteral("ready\n");
  const ssize_t written = ::write(
      slave, output.constData(), static_cast<size_t>(output.size()));
  QVERIFY(written == output.size());
  QVERIFY(QTest::qWaitFor([&sink] { return !sink.empty(); }, kSinkWaitMs));
  QByteArray forwarded;
  for (const auto &chunk : sink) {
    forwarded.append(chunk);
  }
  QVERIFY(forwarded.contains(output));

  // With echo and canonical mode restored, bridge input is echoed back
  // through the master — proof that keyboard bytes take the input direction
  // and the echo renders through the same output channel.
  sink.clear();
  QByteArray echoed;
  termios settings{};
  QVERIFY(::tcgetattr(slave, &settings) == 0);
  settings.c_lflag |= (ECHO | ICANON);
  settings.c_cc[VMIN] = 1;
  QVERIFY(::tcsetattr(slave, TCSANOW, &settings) == 0);
  const QByteArray typed = QByteArrayLiteral("x\r");
  bridge.writeInput(typed.constData(),
                    static_cast<int>(typed.size()));
  QVERIFY(QTest::qWaitFor([&sink, &echoed] {
    for (const auto &chunk : sink) {
      echoed.append(chunk);
    }
    sink.clear();
    return echoed.contains(QLatin1String("x"));
  }, kSinkWaitMs));
  ::close(slave);
  bridge.closeChildChannel();
}

void TerminalPtyBridgeTest::childWindowSizeIsApplied() {
  TerminalPtyBridge bridge([](const char *, int) {}, this);
  const auto opened = bridge.open();
  QVERIFY(opened.ok);
  const int slave = ::open(opened.slavePath.toUtf8().constData(),
                           O_RDWR | O_NOCTTY);
  QVERIFY(slave >= 0);

  bridge.setChildWindowSize(101, 37);
  winsize size{};
  QCOMPARE(::ioctl(slave, TIOCGWINSZ, &size), 0);
  QCOMPARE(int(size.ws_col), 101);
  QCOMPARE(int(size.ws_row), 37);

  // Hostile sizes are clamped, never applied verbatim.
  bridge.setChildWindowSize(0, -5);
  QCOMPARE(::ioctl(slave, TIOCGWINSZ, &size), 0);
  QCOMPARE(int(size.ws_col), 101); // Unchanged: the call was rejected.
  ::close(slave);
  bridge.closeChildChannel();
}

void TerminalPtyBridgeTest::closeStopsForwarding() {
  std::vector<QByteArray> sink;
  TerminalPtyBridge bridge(
      [&sink](const char *data, int length) {
        sink.emplace_back(data, static_cast<int>(length));
      },
      this);
  const auto opened = bridge.open();
  QVERIFY(opened.ok);
  const int slave = ::open(opened.slavePath.toUtf8().constData(),
                           O_RDWR | O_NOCTTY);
  QVERIFY(slave >= 0);
  bridge.closeChildChannel();
  QVERIFY(!bridge.isOpen());

  // After the master close, writes to the bridge are dropped and the sink
  // stays quiet; the slave read side reports EOF/EIO rather than hanging.
  bridge.writeInput("ignored", 7);
  QByteArray chunk(64, Qt::Uninitialized);
  const ssize_t result =
      ::read(slave, chunk.data(), static_cast<size_t>(chunk.size()));
  QVERIFY(result <= 0);
  ::close(slave);
}

QTEST_MAIN(TerminalPtyBridgeTest)
#include "tst_pty_bridge.moc"
