// SPDX-License-Identifier: GPL-3.0-or-later
// The real Main.qml, the real controller and the real input filter on an
// offscreen window, with fake PAM/protocol ports. It reproduces the owner's
// symptom (keyboard-only unlock looks frozen once an attempt ended) at the UI
// level and pins the repaired behavior (ADR-0382): the credential field is
// visible, enabled and focused in every state, the first key press starts the
// attempt without losing the character, and Enter always submits.
#include "authentication_controller.h"
#include "fake_authentication_ports.h"
#include "input_activity_filter.h"
#include "protocol_client.h"
#include "saver_presentation.h"
#include "touch_keyboard.h"
#include <QLibraryInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>
#include <memory>
using namespace QindaQt;
using namespace QindaQt::LockGreeter;
using namespace QindaQt::LockGreeter::TestSupport;

namespace {
class BlankPreferences final : public Session::DesktopControls::ScreensaverPreferencesProvider {
public:
  Session::DesktopControls::ScreensaverPreferences currentPreferences() const override { return {}; }
  void refresh() override {}
};

// Mirrors NativeWindows::create(): same context properties, same filter wiring.
struct Greeter {
  FakeSession session;
  FakeRunner runner;
  AuthenticationController controller{session, runner, QStringLiteral("tester")};
  BlankPreferences preferences;
  SaverPresentation saver{preferences};
  LockProtocol::ProtocolClient inertClient{nullptr, nullptr, 0}; // No Wayland: layout rows stay empty.
  TouchKeyboard keyboard{inertClient};
  InputActivityFilter filter;
  QQuickView view;
  bool loaded = false;
  Greeter() {
    view.engine()->setImportPathList({QStringLiteral("qrc:/qt/qml"), QLibraryInfo::path(QLibraryInfo::QmlImportsPath)});
    QObject::connect(&filter, &InputActivityFilter::activity, &controller, &AuthenticationController::noteInput);
    view.installEventFilter(&filter);
    auto *context = view.rootContext();
    context->setContextProperty(QStringLiteral("authentication"), &controller);
    context->setContextProperty(QStringLiteral("keyboard"), &keyboard.model());
    context->setContextProperty(QStringLiteral("touchKeyboard"), &keyboard);
    context->setContextProperty(QStringLiteral("saver"), &saver);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl(QStringLiteral("qrc:/native-lock/Main.qml")));
    loaded = view.status() == QQuickView::Ready;
    view.resize(900, 700);
  }
  QQuickItem *item(const char *name) const { return view.rootObject()->findChild<QQuickItem *>(QString::fromLatin1(name)); }
  QQuickItem *field() const { return item("nativeLockPassword"); }
  QString text() const { return field()->property("text").toString(); }
  // QTest::keyClicks exists only for widgets; this types ASCII into the window.
  void type(const char *characters) {
    for (const char *c = characters; *c; ++c) QTest::keyClick(&view, *c);
  }
};
} // namespace

class LockGreeterUiTest : public QObject {
  Q_OBJECT
  // The invariant of this whole change, checked after every step.
  static void verifyFieldUsable(Greeter &g) {
    auto *field = g.field();
    QVERIFY(field);
    QVERIFY2(field->isVisible(), "the credential field must never be hidden while locked");
    QVERIFY2(field->isEnabled(), "the credential field must never be disabled while locked");
    QVERIFY2(field->hasActiveFocus(), "the credential field must hold the keyboard focus");
  }
  static std::unique_ptr<Greeter> lockedGreeter() {
    auto g = std::make_unique<Greeter>();
    if (!g->loaded) return g;
    g->view.show();
    g->session.lock();
    return g;
  }
private Q_SLOTS:
  void init() { QTest::failOnWarning(); }
  void fieldIsReadyTheMomentTheScreenLocks() {
    auto g = lockedGreeter();
    QVERIFY2(g->loaded, "Main.qml failed to load");
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    verifyFieldUsable(*g);
    QCOMPARE(g->runner.starts, 0); // Ready, and nothing is talking to PAM.
  }
  void theFirstKeyStartsOneAttemptAndKeepsItsCharacter() {
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    QTest::keyClick(&g->view, Qt::Key_H);
    QCOMPARE(g->runner.starts, 1);
    QCOMPARE(g->text(), QStringLiteral("h")); // The character that woke it is not swallowed.
    g->type("unter2");
    QCOMPARE(g->runner.starts, 1);
    QCOMPARE(g->text(), QStringLiteral("hunter2"));
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QCOMPARE(g->text(), QStringLiteral("hunter2")); // The prompt does not wipe type-ahead.
    verifyFieldUsable(*g);
  }
  void enterSubmitsTheTypedPassword() {
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    g->type("hunter2");
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QTest::keyClick(&g->view, Qt::Key_Return);
    QCOMPARE(g->runner.responses.size(), std::size_t{1});
    QCOMPARE(g->runner.responses[0], QStringLiteral("hunter2"));
    QCOMPARE(g->text(), QString()); // Cleared on submission, never left on screen.
    verifyFieldUsable(*g);
  }
  void enterBeforeThePromptArrivesIsHeldNotLost() {
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    g->type("quick");
    QTest::keyClick(&g->view, Qt::Key_Enter); // PAM has not asked yet.
    QVERIFY(g->runner.responses.empty());
    QCOMPARE(g->text(), QString());
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QCOMPARE(g->runner.responses.size(), std::size_t{1});
    QCOMPARE(g->runner.responses[0], QStringLiteral("quick"));
    verifyFieldUsable(*g);
  }
  void afterATimeoutTheFieldIsStillThereAndEnterStillWorks_data() {
    QTest::addColumn<int>("outcome");
    QTest::newRow("timed-out") << int(Outcome::TimedOut); // The owner's reported case (90 s).
    QTest::newRow("wrong-password") << int(Outcome::Denied);
    QTest::newRow("unavailable") << int(Outcome::Unavailable);
  }
  void afterATimeoutTheFieldIsStillThereAndEnterStillWorks() {
    QFETCH(int, outcome);
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    QTest::keyClick(&g->view, Qt::Key_Shift); // Someone woke the screen (no text typed) ...
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    g->runner.finish(static_cast<Outcome>(outcome)); // ... and the attempt ended.
    verifyFieldUsable(*g); // Before: hidden and disabled, Enter did nothing.
    QVERIFY(!g->controller.busy());
    // Keyboard only: typing starts a fresh attempt, Enter submits to it.
    g->type("again");
    QCOMPARE(g->runner.starts, 2);
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QTest::keyClick(&g->view, Qt::Key_Return);
    QCOMPARE(g->runner.responses.size(), std::size_t{1});
    QCOMPARE(g->runner.responses[0], QStringLiteral("again"));
    verifyFieldUsable(*g);
  }
  void enterWithoutAnyPriorKeyStillSubmits() {
    // A touch keyboard or accessibility submit has no preceding key press.
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    g->field()->setProperty("text", QStringLiteral("direct"));
    QTest::keyClick(&g->view, Qt::Key_Return); // Starts the attempt and submits once prompted.
    QCOMPARE(g->runner.starts, 1);
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QCOMPARE(g->runner.responses.size(), std::size_t{1});
    QCOMPARE(g->runner.responses[0], QStringLiteral("direct"));
  }
  void escapeCancelsAndLeavesAReadyField() {
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    g->type("oops");
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    QTest::keyClick(&g->view, Qt::Key_Escape);
    QVERIFY(g->runner.cancels >= 1);
    QCOMPARE(g->text(), QString());
    verifyFieldUsable(*g);
    QCOMPARE(g->runner.starts, 1); // Escape never starts an attempt by itself.
  }
  void clickingUnlockSubmitsTheField() {
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    g->type("clicked");
    g->runner.offer(MessageKind::Secret, QStringLiteral("Password: "));
    auto *unlock = g->item("nativeLockUnlock");
    QVERIFY(unlock);
    QVERIFY(unlock->isEnabled()); // Never disabled while locked.
    const QPoint where = unlock->mapToScene(QPointF(unlock->width() / 2, unlock->height() / 2)).toPoint();
    QTest::mouseClick(&g->view, Qt::LeftButton, Qt::NoModifier, where);
    QCOMPARE(g->runner.responses.size(), std::size_t{1});
    QCOMPARE(g->runner.responses[0], QStringLiteral("clicked"));
    verifyFieldUsable(*g);
  }
  void aClickAloneStartsTheAttempt() {
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    QTest::mouseClick(&g->view, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5)); // Bare background.
    QCOMPARE(g->runner.starts, 1);
    verifyFieldUsable(*g);
  }
  void pointerMotionAloneStartsNothing() {
    auto g = lockedGreeter();
    QVERIFY(g->loaded);
    QVERIFY(QTest::qWaitForWindowActive(&g->view));
    QTest::mouseMove(&g->view, QPoint(50, 50));
    QTest::mouseMove(&g->view, QPoint(60, 70));
    QCOMPARE(g->runner.starts, 0);
  }
};
QTEST_MAIN(LockGreeterUiTest)
#include "tst_lock_greeter_ui.moc"
