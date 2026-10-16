// SPDX-License-Identifier: GPL-3.0-or-later
// The REAL Qt Wayland client with the REAL ext-session-lock shell integration
// plugin, against a fake compositor that scripts unusable configure events. It
// pins ADR-0382: a 0x0 (or otherwise unusable) configure used to qFatal the
// locker and leave the session on a black screen. Now it is skipped (never
// acknowledged, never applied), the process survives, and the next usable
// configure is acknowledged and committed normally.
#include "fake_lock_compositor.h"
#include "protocol_client.h"
#include <QBackingStore>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QtGui/qscreen_platform.h>
#include <QtTest>
#include <qpa/qplatformnativeinterface.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
using namespace QindaQt::LockProtocol;
using QindaQt::LockProtocol::Testing::FakeLockCompositor;
using Kind = FakeLockCompositor::Observation::Kind;

namespace {
class PaintedWindow final : public QWindow {
public:
  PaintedWindow() : m_store(this) {}
protected:
  void exposeEvent(QExposeEvent *) override { render(); }
  void resizeEvent(QResizeEvent *) override { render(); }
private:
  void render() {
    if (!isExposed() || size().isEmpty()) return;
    m_store.resize(size());
    m_store.beginPaint(QRegion(QRect(QPoint(), size())));
    QPainter painter(m_store.paintDevice());
    painter.fillRect(QRect(QPoint(), size()), QColor("#345678"));
    painter.end();
    m_store.endPaint();
    m_store.flush(QRegion(QRect(QPoint(), size())));
  }
  QBackingStore m_store;
};
} // namespace

class SessionLockIntegrationTest : public QObject {
  Q_OBJECT
public:
  explicit SessionLockIntegrationTest(FakeLockCompositor &compositor) : m_compositor(compositor) {}
private:
  std::vector<std::uint32_t> acks() const {
    std::vector<std::uint32_t> serials;
    for (const auto &entry : m_compositor.observations())
      if (entry.kind == Kind::AckConfigure) serials.push_back(entry.serial);
    return serials;
  }
  QSize lastCommit() const {
    QSize size;
    for (const auto &entry : m_compositor.observations())
      if (entry.kind == Kind::Commit) size = QSize(entry.width, entry.height);
    return size;
  }
  // Sends a configure the integration must skip, and gives the client time to see it.
  void sendUnusable(unsigned width, unsigned height) {
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("ignoring unusable lock surface configure %1x%2")
                                                              .arg(width).arg(height)));
    QVERIFY(m_compositor.sendConfigure(width, height) != 0);
    // The client logs the warning when it dispatches the event. ignoreMessage
    // makes the test fail at its end if that line never appeared, so the reason
    // is guaranteed to reach the journal.
    QTest::qWait(150);
  }
private Q_SLOTS:
  void unusableConfiguresAreSkippedAndTheNextUsableOneIsHonoured() {
    QScreen *screen = nullptr;
    for (auto *candidate : QGuiApplication::screens()) {
      const auto *native = candidate->nativeInterface<QNativeInterface::QWaylandScreen>();
      if (native && native->output()) { screen = candidate; break; }
    }
    QVERIFY2(screen, "the fake compositor's wl_output never became a QScreen");
    PaintedWindow window;
    window.setScreen(screen);
    window.setFlags(Qt::Window | Qt::FramelessWindowHint);
    window.setGeometry(screen->geometry());
    window.create();
    auto *client = static_cast<ProtocolClient *>(
        QGuiApplication::platformNativeInterface()->nativeResourceForWindow(nativeResource, &window));
    QVERIFY2(client, "the session-lock role could not be assigned");
    window.show();
    QTRY_COMPARE(m_compositor.count(Kind::LockSurfaceRequested), std::size_t{1});

    // 1. Before any usable configure: a 0x0 configure must not abort, must not be
    //    acknowledged and must not produce a buffer.
    sendUnusable(0, 0);
    QCOMPARE(m_compositor.count(Kind::AckConfigure), std::size_t{0});
    QCOMPARE(m_compositor.count(Kind::Commit), std::size_t{0});
    sendUnusable(0, 480);
    sendUnusable(0x80000000U, 100); // Beyond what a QSize holds.
    QCOMPARE(m_compositor.count(Kind::AckConfigure), std::size_t{0});

    // 2. The next usable configure is acknowledged by its own serial and committed at its size.
    const auto first = m_compositor.sendConfigure(640, 480);
    QTRY_COMPARE(acks(), std::vector<std::uint32_t>{first});
    QTRY_COMPARE(lastCommit(), QSize(640, 480));

    // 3. A later unusable configure leaves the surface as it was ...
    sendUnusable(0, 0);
    QCOMPARE(acks().size(), std::size_t{1});
    QCOMPARE(lastCommit(), QSize(640, 480));
    // ... and a usable one after it resizes normally.
    const auto second = m_compositor.sendConfigure(800, 600);
    QTRY_COMPARE(acks(), (std::vector<std::uint32_t>{first, second}));
    QTRY_COMPARE(lastCommit(), QSize(800, 600));
    QVERIFY(window.isExposed());
  }
  void theObserverResourceAssignsNoRole() {
    auto window = makeWindow();
    QVERIFY(window);
    const auto before = m_compositor.count(Kind::LockSurfaceRequested);
    auto *observer = static_cast<ProtocolClient *>(
        QGuiApplication::platformNativeInterface()->nativeResourceForWindow(nativeObserverResource, window.get()));
    QVERIFY(observer);
    QTest::qWait(100);
    QCOMPARE(m_compositor.count(Kind::LockSurfaceRequested), before); // Observing created no protocol object.
    auto *client = static_cast<ProtocolClient *>(
        QGuiApplication::platformNativeInterface()->nativeResourceForWindow(nativeResource, window.get()));
    QCOMPARE(client, observer); // Same borrowed client; only this call assigns the role.
    QTRY_COMPARE(m_compositor.count(Kind::LockSurfaceRequested), before + 1);
  }
  void aRoleOfAWindowThatDiesUnshownIsReleased() {
    // The greeter creates the window and its role, then loads QML; if that fails
    // the view is deleted before it was ever shown. The output must be usable again.
    auto *protocol = QGuiApplication::platformNativeInterface();
    const auto requested = m_compositor.count(Kind::LockSurfaceRequested);
    const auto destroyed = m_compositor.count(Kind::LockSurfaceDestroyed);
    {
      auto first = makeWindow();
      QVERIFY(first);
      QVERIFY(protocol->nativeResourceForWindow(nativeResource, first.get()));
      QTRY_COMPARE(m_compositor.count(Kind::LockSurfaceRequested), requested + 1);
    } // Destroyed without ever being shown.
    QTRY_COMPARE(m_compositor.count(Kind::LockSurfaceDestroyed), destroyed + 1); // The protocol role is gone ...
    auto second = makeWindow();
    QVERIFY(second);
    QVERIFY2(protocol->nativeResourceForWindow(nativeResource, second.get()),
             "the output stayed blocked by the role of a window that no longer exists"); // ... so a retry can succeed.
    QTRY_COMPARE(m_compositor.count(Kind::LockSurfaceRequested), requested + 2);
  }
  // Must run last: finishing the lock ends it for the whole process.
  void aFinishedLockRefusesRolesButStaysObservable() {
    auto window = makeWindow();
    QVERIFY(window);
    auto *observer = static_cast<ProtocolClient *>(
        QGuiApplication::platformNativeInterface()->nativeResourceForWindow(nativeObserverResource, window.get()));
    QVERIFY(observer);
    QVERIFY(observer->available());
    QSignalSpy rejected(observer, &ProtocolClient::rejected);
    m_compositor.sendFinished();
    QTRY_COMPARE(rejected.size(), 1); // The greeter exits on this; it must still be delivered.
    QVERIFY(!observer->available());
    // A window created now is refused its role, yet the lock stays observable so
    // the locker can see it is finished instead of retrying forever.
    auto late = makeWindow();
    QVERIFY(late);
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("no lock role: the session lock is no longer available")));
    QVERIFY(!QGuiApplication::platformNativeInterface()->nativeResourceForWindow(nativeResource, late.get()));
    QCOMPARE(QGuiApplication::platformNativeInterface()->nativeResourceForWindow(nativeObserverResource, late.get()),
             static_cast<void *>(observer));
  }
private:
  // A frameless window on the fake compositor's output, not yet shown or given a role.
  std::unique_ptr<PaintedWindow> makeWindow() {
    for (auto *candidate : QGuiApplication::screens()) {
      const auto *native = candidate->nativeInterface<QNativeInterface::QWaylandScreen>();
      if (!native || !native->output()) continue;
      auto window = std::make_unique<PaintedWindow>();
      window->setScreen(candidate);
      window->setFlags(Qt::Window | Qt::FramelessWindowHint);
      window->setGeometry(candidate->geometry());
      window->create();
      return window;
    }
    return nullptr;
  }
  FakeLockCompositor &m_compositor;
};

int main(int argc, char **argv) {
  char runtimeTemplate[] = "/tmp/qindaqt-lock-test-XXXXXX";
  if (!mkdtemp(runtimeTemplate)) return 77;
  const std::string runtime = runtimeTemplate;
  qputenv("XDG_RUNTIME_DIR", runtime.c_str()); // mkdtemp creates it 0700, which Qt requires.
  qputenv("WAYLAND_DISPLAY", "qindaqt-lock-test-0");
  qunsetenv("WAYLAND_SOCKET");
  qputenv("QT_QPA_PLATFORM", "wayland");
  qputenv("QT_WAYLAND_SHELL_INTEGRATION", "qindaqt-session-lock");
  int result = 1;
  {
    FakeLockCompositor compositor;
    if (!compositor.start("qindaqt-lock-test-0")) {
      std::fprintf(stderr, "cannot start the fake compositor\n");
    } else {
      // Compile-time private fixture path only, never a production import option.
      QCoreApplication::setLibraryPaths({QStringLiteral(QINDAQT_PRIVATE_LOCK_PLUGIN_ROOT),
                                         QLibraryInfo::path(QLibraryInfo::PluginsPath)});
      QGuiApplication app(argc, argv);
      if (app.platformName() != QStringLiteral("wayland")) {
        std::fprintf(stderr, "the wayland platform plugin is unavailable\n");
        result = 77;
      } else {
        SessionLockIntegrationTest test(compositor);
        result = QTest::qExec(&test, argc, argv);
      }
    }
  }
  std::filesystem::remove_all(runtime);
  return result;
}
#include "tst_session_lock_integration.moc"
