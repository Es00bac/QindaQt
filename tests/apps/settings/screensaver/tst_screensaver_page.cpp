// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtGui/QAccessible>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtTest>

#include <memory>

namespace {

QQuickItem *findItem(QQuickItem *root, const QString &name) {
  if (root == nullptr) return nullptr;
  if (root->objectName() == name) return root;
  for (QQuickItem *child : root->childItems())
    if (QQuickItem *found = findItem(child, name)) return found;
  return nullptr;
}

// The saver model, stubbed at its QML surface: the real one is proven against
// the fake transport in qindaqt.settings-screensaver-model, so this stub only
// has to carry the same property/invokable contract the page binds to.
class StubScreensaverSettings final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList saverOptions READ saverOptions NOTIFY changed)
  Q_PROPERTY(QString saver MEMBER saver NOTIFY changed)
  Q_PROPERTY(bool delayEnabled MEMBER delayEnabled NOTIFY changed)
  Q_PROPERTY(int minutes MEMBER minutes NOTIFY changed)
  Q_PROPERTY(bool hasConfirmed MEMBER hasConfirmed NOTIFY changed)
  Q_PROPERTY(bool available MEMBER available NOTIFY changed)
  Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY changed)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY changed)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY changed)
  Q_PROPERTY(bool previewAvailable MEMBER previewAvailable NOTIFY changed)
  Q_PROPERTY(bool previewRunning MEMBER previewRunning NOTIFY changed)
  Q_PROPERTY(QString previewSummary MEMBER previewSummary NOTIFY changed)

public:
  using QObject::QObject;

  QString saver = QStringLiteral("circuit-reef");
  bool delayEnabled = true;
  int minutes = 5;
  bool hasConfirmed = true;
  bool available = true;
  bool canEdit = true;
  bool busy = false;
  QString statusText = QStringLiteral("Circuit Reef starts after 5 minutes of inactivity.");
  QString errorText;
  bool previewAvailable = true;
  bool previewRunning = false;
  QString previewSummary = QStringLiteral(
      "Runs Circuit Reef itself with the catalog arguments used by the idle path.");
  QStringList saverRequests;
  QList<int> minutesRequests;
  int retryCalls = 0;
  int previewCalls = 0;
  bool admitWrites = true;

  [[nodiscard]] QVariantList saverOptions() const {
    return {
        QVariantMap{{QStringLiteral("token"), QStringLiteral("none")},
                    {QStringLiteral("name"), QStringLiteral("None")}},
        QVariantMap{{QStringLiteral("token"), QStringLiteral("blank")},
                    {QStringLiteral("name"), QStringLiteral("Blank screen")}},
        QVariantMap{{QStringLiteral("token"), QStringLiteral("circuit-reef")},
                    {QStringLiteral("name"), QStringLiteral("Circuit Reef")}},
        QVariantMap{{QStringLiteral("token"), QStringLiteral("prism-brawl")},
                    {QStringLiteral("name"), QStringLiteral("Prism Brawl")}},
        QVariantMap{{QStringLiteral("token"), QStringLiteral("qinda-patrol")},
                    {QStringLiteral("name"), QStringLiteral("Qinda Patrol")}},
    };
  }

  Q_INVOKABLE bool setSaver(const QString &value) {
    saverRequests.append(value);
    if (!admitWrites) return false;
    saver = value;
    delayEnabled = value != QStringLiteral("none") && value != QStringLiteral("blank");
    Q_EMIT changed();
    return true;
  }
  Q_INVOKABLE bool setMinutes(int value) {
    minutesRequests.append(value);
    if (!admitWrites) return false;
    minutes = value;
    Q_EMIT changed();
    return true;
  }
  Q_INVOKABLE bool retry() { ++retryCalls; return true; }
  Q_INVOKABLE bool preview() { ++previewCalls; return true; }

Q_SIGNALS:
  void changed();
  void saversChanged();
};

// The shared walk-away lock model, stubbed the same way.
class StubScreenLockSettings final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool automaticLock MEMBER automaticLock NOTIFY changed)
  Q_PROPERTY(int timeoutMinutes MEMBER timeoutMinutes NOTIFY changed)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY changed)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY changed)

public:
  using QObject::QObject;

  bool automaticLock = true;
  int timeoutMinutes = 5;
  bool busy = false;
  QString statusText = QStringLiteral("The screen locks after 5 minutes.");
  QString errorText;
  QList<bool> automaticLockRequests;
  QList<int> timeoutRequests;
  int retryCalls = 0;

  Q_INVOKABLE bool setAutomaticLock(bool enabled) {
    automaticLockRequests.append(enabled);
    automaticLock = enabled;
    Q_EMIT changed();
    return true;
  }
  Q_INVOKABLE bool setTimeoutMinutes(int minutes) {
    timeoutRequests.append(minutes);
    timeoutMinutes = minutes;
    Q_EMIT changed();
    return true;
  }
  Q_INVOKABLE bool retryLiveApply() { ++retryCalls; return true; }

Q_SIGNALS:
  void changed();
};

} // namespace

class ScreensaverPageTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void rendersChoicesAndReflectsTruth();
  void delayRowDisablesWhenNothingRuns();
  void previewButtonDisablesWhileWriteIsPending();
  void selectingSaverWritesTheToken();
  void unavailableChoiceHasNoFalseSelection();
  void keyboardRefusalRestoresConfirmedChoices();
  void lockSectionIsSeparateAndWrites();
  void errorsSurfaceWithRetry();

private:
  std::unique_ptr<QQuickView> m_view;
  std::unique_ptr<StubScreensaverSettings> m_screensaver;
  std::unique_ptr<StubScreenLockSettings> m_screenLock;
  std::pair<std::unique_ptr<QObject>, QQuickItem *> createPage(QSize size);
};

void ScreensaverPageTest::initTestCase() {
  m_view = std::make_unique<QQuickView>();
  m_view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
  QString error;
  auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
      *m_view->engine(), &error);
  QVERIFY2(facade != nullptr, qPrintable(error));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(loaded.ok, qPrintable(loaded.error));
  QVERIFY2(facade->publish(loaded.theme, {}, &error), qPrintable(error));
}

std::pair<std::unique_ptr<QObject>, QQuickItem *>
ScreensaverPageTest::createPage(const QSize size) {
  m_screensaver = std::make_unique<StubScreensaverSettings>();
  m_screenLock = std::make_unique<StubScreenLockSettings>();
  QQmlComponent component(m_view->engine());
  component.loadUrl(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SCREENSAVER_PAGE_QML_PATH)));
  if (!component.isReady()) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  QObject *object = component.createWithInitialProperties({
      {QStringLiteral("screensaverSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_screensaver.get()))},
      {QStringLiteral("screenLockSettings"),
       QVariant::fromValue(static_cast<QObject *>(m_screenLock.get()))},
  });
  if (object == nullptr) {
    qWarning().noquote() << component.errorString();
    return {};
  }
  auto guard = std::unique_ptr<QObject>(object);
  auto *page = qobject_cast<QQuickItem *>(object);
  if (page == nullptr) return {};
  m_view->resize(size);
  page->setParentItem(m_view->contentItem());
  page->setSize(size);
  m_view->show();
  QCoreApplication::processEvents();
  return {std::move(guard), page};
}

void ScreensaverPageTest::rendersChoicesAndReflectsTruth() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);

  auto *selector = findItem(page, QStringLiteral("screensaverSaverSelector"));
  QVERIFY(selector != nullptr);
  // The chosen saver is the one shown, out of the discovered list.
  QCOMPARE(selector->property("currentText").toString(),
           QStringLiteral("Circuit Reef"));
  QCOMPARE(selector->property("count").toInt(), 5);

  auto *status = findItem(page, QStringLiteral("screensaverStatus"));
  QVERIFY(status != nullptr);
  QVERIFY(status->property("text").toString().contains(QStringLiteral("Circuit Reef")));

  auto *preview = findItem(page, QStringLiteral("screensaverPreviewButton"));
  QVERIFY(preview != nullptr);
  QVERIFY(preview->isVisible());
  QVERIFY(preview->isEnabled());
  auto *previewSummary =
      findItem(page, QStringLiteral("screensaverPreviewSummary"));
  QVERIFY(previewSummary != nullptr);
  QVERIFY(previewSummary->property("text").toString().contains(
      QStringLiteral("catalog arguments used by the idle path")));

  // The note keeps the two concepts visibly separate.
  auto *note = findItem(page, QStringLiteral("screensaverNote"));
  QVERIFY(note != nullptr);
  QVERIFY(note->property("text").toString().contains(QStringLiteral("does not lock")));

  auto *lockSwitch = findItem(page, QStringLiteral("screensaverAutomaticScreenLock"));
  QVERIFY(lockSwitch != nullptr);
  QVERIFY(lockSwitch->property("checked").toBool());

  // The page is accessible: the heading is a real heading and the saver
  // selector carries its description.
  auto *heading = findItem(page, QStringLiteral("screensaverPageHeading"));
  QVERIFY(heading != nullptr);
  auto *headingAccessible = QAccessible::queryAccessibleInterface(heading);
  QVERIFY(headingAccessible != nullptr);
  QCOMPARE(headingAccessible->role(), QAccessible::Heading);
  auto *selectorAccessible = QAccessible::queryAccessibleInterface(selector);
  QVERIFY(selectorAccessible != nullptr);
  QVERIFY(!selectorAccessible->text(QAccessible::Description).isEmpty());
}

void ScreensaverPageTest::delayRowDisablesWhenNothingRuns() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);

  m_screensaver->setSaver(QStringLiteral("none"));
  QCoreApplication::processEvents();

  auto *delay = findItem(page, QStringLiteral("screensaverDelaySelector"));
  QVERIFY(delay != nullptr);
  QTRY_VERIFY(!delay->isEnabled());

  auto *preview = findItem(page, QStringLiteral("screensaverPreviewButton"));
  QVERIFY(preview != nullptr);
  m_screensaver->previewAvailable = false;
  Q_EMIT m_screensaver->changed();
  QCoreApplication::processEvents();
  QVERIFY(!preview->isVisible());
}

void ScreensaverPageTest::previewButtonDisablesWhileWriteIsPending() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);

  auto *preview = findItem(page, QStringLiteral("screensaverPreviewButton"));
  QVERIFY(preview != nullptr);
  QVERIFY(preview->isEnabled());

  m_screensaver->busy = true;
  Q_EMIT m_screensaver->changed();
  QCoreApplication::processEvents();
  QVERIFY(!preview->isEnabled());
}

void ScreensaverPageTest::selectingSaverWritesTheToken() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);

  auto *selector = findItem(page, QStringLiteral("screensaverSaverSelector"));
  QVERIFY(selector != nullptr);
  selector->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), selector);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);

  // Circuit Reef (index 2) moves to Prism Brawl; the written value is the
  // discovered token, not the display name.
  QCOMPARE(m_screensaver->saverRequests.size(), 1);
  QCOMPARE(m_screensaver->saverRequests.constFirst(), QStringLiteral("prism-brawl"));
  QTRY_COMPARE(selector->property("currentIndex").toInt(), 3);

  auto *delay = findItem(page, QStringLiteral("screensaverDelaySelector"));
  QVERIFY(delay != nullptr);
  delay->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), delay);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QCOMPARE(m_screensaver->minutesRequests.size(), 1);
  QTRY_COMPARE(delay->property("currentIndex").toInt(), 3);
}

void ScreensaverPageTest::unavailableChoiceHasNoFalseSelection() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  m_screensaver->hasConfirmed = false;
  m_screensaver->available = false;
  m_screensaver->canEdit = false;
  m_screensaver->saver.clear();
  m_screensaver->minutes = 0;
  m_screensaver->statusText = QStringLiteral("Waiting for confirmed screen saver settings.");
  Q_EMIT m_screensaver->changed();
  QCoreApplication::processEvents();
  auto *saver = findItem(page, QStringLiteral("screensaverSaverSelector"));
  auto *delay = findItem(page, QStringLiteral("screensaverDelaySelector"));
  auto *status = findItem(page, QStringLiteral("screensaverStatus"));
  QVERIFY(saver != nullptr && delay != nullptr && status != nullptr);
  QCOMPARE(saver->property("currentIndex").toInt(), -1);
  QVERIFY(!saver->isEnabled());
  QVERIFY(!delay->isEnabled());
  QVERIFY(status->property("text").toString().contains(QStringLiteral("Waiting")));
  m_screensaver->hasConfirmed = true;
  m_screensaver->available = true;
  m_screensaver->canEdit = true;
  m_screensaver->saver = QStringLiteral("none");
  m_screensaver->minutes = 5;
  Q_EMIT m_screensaver->changed();
  QTRY_COMPARE(saver->property("currentIndex").toInt(), 0);
}

void ScreensaverPageTest::keyboardRefusalRestoresConfirmedChoices() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);
  m_screensaver->admitWrites = false;
  auto *saver = findItem(page, QStringLiteral("screensaverSaverSelector"));
  auto *delay = findItem(page, QStringLiteral("screensaverDelaySelector"));
  QVERIFY(saver != nullptr && delay != nullptr);
  QCOMPARE(saver->property("currentIndex").toInt(), 2);
  saver->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_screensaver->saverRequests.size(), 1);
  QTRY_COMPARE(saver->property("currentIndex").toInt(), 2);
  QCOMPARE(m_screensaver->saver, QStringLiteral("circuit-reef"));
  delay->forceActiveFocus(Qt::TabFocusReason);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QTRY_COMPARE(m_screensaver->minutesRequests.size(), 1);
  QTRY_COMPARE(delay->property("currentIndex").toInt(), 2);
  QCOMPARE(m_screensaver->minutes, 5);
}

void ScreensaverPageTest::lockSectionIsSeparateAndWrites() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);

  // The walk-away switch writes the lock model, never the saver model.
  auto *lockSwitch = findItem(page, QStringLiteral("screensaverAutomaticScreenLock"));
  QVERIFY(lockSwitch != nullptr);
  lockSwitch->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), lockSwitch);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QCOMPARE(m_screenLock->automaticLockRequests.size(), 1);
  QCOMPARE(m_screenLock->automaticLockRequests.constFirst(), false);
  QVERIFY(m_screensaver->saverRequests.isEmpty());

  // Turning locking off disables the lock timeout row; the saver delay is
  // untouched.
  auto *timeout = findItem(page, QStringLiteral("screensaverLockTimeoutSelector"));
  QVERIFY(timeout != nullptr);
  QCoreApplication::processEvents();
  QTRY_VERIFY(!timeout->isEnabled());

  m_screenLock->setAutomaticLock(true);
  QCoreApplication::processEvents();
  QTRY_VERIFY(timeout->isEnabled());
  timeout->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), timeout);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QTest::keyClick(m_view.get(), Qt::Key_Down);
  QTest::keyClick(m_view.get(), Qt::Key_Return);
  QCOMPARE(m_screenLock->timeoutRequests.size(), 1);
}

void ScreensaverPageTest::errorsSurfaceWithRetry() {
  auto [guard, page] = createPage(QSize(900, 700));
  QVERIFY(page != nullptr);

  m_screensaver->errorText = QStringLiteral("The screensaver preference could not be applied.");
  Q_EMIT m_screensaver->changed();
  QCoreApplication::processEvents();

  auto *error = findItem(page, QStringLiteral("screensaverError"));
  QVERIFY(error != nullptr);
  QVERIFY(error->isVisible());
  auto *retry = findItem(page, QStringLiteral("screensaverRetry"));
  QVERIFY(retry != nullptr);
  QVERIFY(retry->isVisible());
  retry->forceActiveFocus(Qt::TabFocusReason);
  QTRY_COMPARE(m_view->activeFocusItem(), retry);
  QTest::keyClick(m_view.get(), Qt::Key_Space);
  QCOMPARE(m_screensaver->retryCalls, 1);

  // The lock section reports its own failures separately.
  m_screenLock->errorText = QStringLiteral("Screen-lock settings are unavailable.");
  Q_EMIT m_screenLock->changed();
  QCoreApplication::processEvents();
  auto *lockError = findItem(page, QStringLiteral("screensaverLockError"));
  QVERIFY(lockError != nullptr);
  QVERIFY(lockError->isVisible());
  QVERIFY(lockError->property("text").toString().contains(QStringLiteral("unavailable")));
}

QTEST_MAIN(ScreensaverPageTest)
#include "tst_screensaver_page.moc"
