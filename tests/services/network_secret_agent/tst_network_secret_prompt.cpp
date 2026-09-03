// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/services/network_secret_agent/qml_prompt_presenter.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtGui/QAccessible>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

#include <memory>

using namespace QindaQt::Network::SecretAgent;

namespace {

bool publishTokens(QQmlEngine &engine) {
  engine.addImportPath(QStringLiteral(QINDAQT_TEST_QML_IMPORT_PATH));
  QQmlComponent registration(&engine);
  registration.setData(
      R"qml(
    import QtQuick
    import QindaQt.Tokens 1.0
    QtObject { property int revision: Tokens.qstRevision }
  )qml",
      QUrl(QStringLiteral("inline:secret-prompt-test-tokens.qml")));
  if (registration.status() == QQmlComponent::Loading) {
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&registration, &QQmlComponent::statusChanged, &loop,
                     [&loop](const QQmlComponent::Status status) {
                       if (status != QQmlComponent::Loading) {
                         loop.quit();
                       }
                     });
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    deadline.start(5'000);
    loop.exec();
  }
  if (!registration.isReady()) {
    return false;
  }
  std::unique_ptr<QObject> object(registration.create());
  auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
      QStringLiteral("QindaQt.Tokens"), QStringLiteral("Tokens"));
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_TEST_THEME_PATH));
  QString error;
  return object != nullptr && facade != nullptr && theme.ok &&
         facade->publish(theme.theme, {}, &error);
}

QQuickWindow *promptWindow() {
  for (QWindow *window : QGuiApplication::allWindows()) {
    if (window->objectName() == QStringLiteral("networkSecretPrompt")) {
      return qobject_cast<QQuickWindow *>(window);
    }
  }
  return nullptr;
}

QQuickItem *findItem(QQuickItem *root, const QString &objectName) {
  if (root == nullptr) {
    return nullptr;
  }
  if (root->objectName() == objectName) {
    return root;
  }
  for (QQuickItem *child : root->childItems()) {
    if (QQuickItem *match = findItem(child, objectName); match != nullptr) {
      return match;
    }
  }
  return nullptr;
}

} // namespace

class NetworkSecretPromptTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void showsBoundedAccessibleFieldsAndEscapeCancels();
  void traversesControlsAndExposesCheckboxStates();
  void enterSubmitsAndWindowCloseCancels();
  void submitsAndClearsEditors();

private:
  QQmlEngine m_engine;
  std::unique_ptr<QmlPromptPresenter> m_presenter;
};

void NetworkSecretPromptTest::initTestCase() {
  QVERIFY2(publishTokens(m_engine), "could not publish QST tokens");
  m_presenter = std::make_unique<QmlPromptPresenter>(m_engine);
}

void NetworkSecretPromptTest::showsBoundedAccessibleFieldsAndEscapeCancels() {
  PromptResult result;
  bool completed = false;
  QVERIFY(m_presenter->showPrompt(
      {1,
       QStringLiteral("Office Wi-Fi"),
       QStringLiteral("/org/freedesktop/NetworkManager/Settings/7"),
       QStringLiteral("802-11-wireless-security"),
       {{QStringLiteral("psk"), QStringLiteral("Wi-Fi password"), true, 64}}},
      [&result, &completed](PromptResult value) {
        result = std::move(value);
        completed = true;
      }));
  QTRY_VERIFY(promptWindow() != nullptr);
  QQuickWindow *window = promptWindow();
  auto *field =
      findItem(window->contentItem(), QStringLiteral("networkSecretField-psk"));
  auto *show =
      findItem(window->contentItem(), QStringLiteral("networkSecretShow"));
  auto *remember =
      findItem(window->contentItem(), QStringLiteral("networkSecretRemember"));
  QVERIFY(field != nullptr);
  QVERIFY(show != nullptr);
  QVERIFY(remember != nullptr);
  QCOMPARE(field->property("maximumLength").toInt(), 64);
  QCOMPARE(field->property("accessibleName").toString(),
           QStringLiteral("Wi-Fi password"));
  window->requestActivate();
  QTRY_VERIFY(window->isActive());
  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_VERIFY(completed);
  QVERIFY(!result.accepted);
}

void NetworkSecretPromptTest::traversesControlsAndExposesCheckboxStates() {
  // AGENT-NOTE: Raman P3-1 found f06d2fd's proof table broader than its
  // direct coverage. Exercise Tab/Shift+Tab, echo state, and checkbox a11y.
  bool completed = false;
  QVERIFY(m_presenter->showPrompt(
      {3,
       QStringLiteral("Keyboard Wi-Fi"),
       QStringLiteral("/org/freedesktop/NetworkManager/Settings/9"),
       QStringLiteral("802-11-wireless-security"),
       {{QStringLiteral("psk"), QStringLiteral("Wi-Fi password"), true, 64}}},
      [&completed](PromptResult value) {
        value.wipe();
        completed = true;
      }));
  QTRY_VERIFY(promptWindow() != nullptr);
  QQuickWindow *window = promptWindow();
  auto *field =
      findItem(window->contentItem(), QStringLiteral("networkSecretField-psk"));
  auto *show =
      findItem(window->contentItem(), QStringLiteral("networkSecretShow"));
  auto *remember =
      findItem(window->contentItem(), QStringLiteral("networkSecretRemember"));
  QVERIFY(field != nullptr);
  QVERIFY(show != nullptr);
  QVERIFY(remember != nullptr);

  window->requestActivate();
  QTRY_VERIFY(window->isActive());
  field->forceActiveFocus();
  QVERIFY(field->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_VERIFY(show->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_Tab);
  QTRY_VERIFY(remember->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_Tab, Qt::ShiftModifier);
  QTRY_VERIFY(show->hasActiveFocus());
  QTest::keyClick(window, Qt::Key_Tab, Qt::ShiftModifier);
  QTRY_VERIFY(field->hasActiveFocus());

  QAccessibleInterface *showAccessible =
      QAccessible::queryAccessibleInterface(show);
  QAccessibleInterface *rememberAccessible =
      QAccessible::queryAccessibleInterface(remember);
  QVERIFY(showAccessible != nullptr);
  QVERIFY(rememberAccessible != nullptr);
  QCOMPARE(showAccessible->role(), QAccessible::CheckBox);
  QCOMPARE(rememberAccessible->role(), QAccessible::CheckBox);
  QVERIFY(!showAccessible->state().checked);
  QVERIFY(!rememberAccessible->state().checked);
  const int concealedEchoMode = field->property("echoMode").toInt();
  QVERIFY(show->setProperty("checked", true));
  QVERIFY(remember->setProperty("checked", true));
  QTRY_VERIFY(showAccessible->state().checked);
  QTRY_VERIFY(rememberAccessible->state().checked);
  QTRY_VERIFY(field->property("echoMode").toInt() != concealedEchoMode);
  QVERIFY(show->setProperty("checked", false));
  QTRY_COMPARE(field->property("echoMode").toInt(), concealedEchoMode);

  QTest::keyClick(window, Qt::Key_Escape);
  QTRY_VERIFY(completed);
}

void NetworkSecretPromptTest::enterSubmitsAndWindowCloseCancels() {
  PromptResult submitted;
  bool completed = false;
  bool editorClearedAtCompletion = false;
  QPointer<QQuickItem> observedField;
  QVERIFY(m_presenter->showPrompt(
      {4,
       QStringLiteral("Enter Wi-Fi"),
       QStringLiteral("/org/freedesktop/NetworkManager/Settings/10"),
       QStringLiteral("802-11-wireless-security"),
       {{QStringLiteral("psk"), QStringLiteral("Wi-Fi password"), true, 64}}},
      [&submitted, &completed, &editorClearedAtCompletion,
       &observedField](PromptResult value) {
        editorClearedAtCompletion =
            observedField != nullptr &&
            observedField->property("text").toString().isEmpty();
        submitted = std::move(value);
        completed = true;
      }));
  QTRY_VERIFY(promptWindow() != nullptr);
  QQuickWindow *window = promptWindow();
  auto *field =
      findItem(window->contentItem(), QStringLiteral("networkSecretField-psk"));
  QVERIFY(field != nullptr);
  observedField = field;
  window->requestActivate();
  QTRY_VERIFY(window->isActive());
  field->setProperty("text", QStringLiteral("enter-canary"));
  field->forceActiveFocus();
  QTest::keyClick(window, Qt::Key_Return);
  QTRY_VERIFY(completed);
  QVERIFY(submitted.accepted);
  QCOMPARE(submitted.values.first().bytes, QByteArray("enter-canary"));
  QVERIFY(editorClearedAtCompletion);
  submitted.wipe();
  QTRY_VERIFY(promptWindow() == nullptr);

  PromptResult closed;
  completed = false;
  editorClearedAtCompletion = false;
  observedField.clear();
  QVERIFY(m_presenter->showPrompt(
      {5,
       QStringLiteral("Close Wi-Fi"),
       QStringLiteral("/org/freedesktop/NetworkManager/Settings/11"),
       QStringLiteral("802-11-wireless-security"),
       {{QStringLiteral("psk"), QStringLiteral("Wi-Fi password"), true, 64}}},
      [&closed, &completed, &editorClearedAtCompletion,
       &observedField](PromptResult value) {
        editorClearedAtCompletion =
            observedField != nullptr &&
            observedField->property("text").toString().isEmpty();
        closed = std::move(value);
        completed = true;
      }));
  QTRY_VERIFY(promptWindow() != nullptr);
  window = promptWindow();
  field =
      findItem(window->contentItem(), QStringLiteral("networkSecretField-psk"));
  QVERIFY(field != nullptr);
  observedField = field;
  field->setProperty("text", QStringLiteral("close-canary"));
  window->close();
  QTRY_VERIFY(completed);
  QVERIFY(!closed.accepted);
  QVERIFY(editorClearedAtCompletion);
}

void NetworkSecretPromptTest::submitsAndClearsEditors() {
  PromptResult result;
  bool completed = false;
  QVERIFY(m_presenter->showPrompt(
      {2,
       QStringLiteral("Enterprise"),
       QStringLiteral("/org/freedesktop/NetworkManager/Settings/8"),
       QStringLiteral("802-1x"),
       {{QStringLiteral("identity"), QStringLiteral("Identity"), false, 253},
        {QStringLiteral("password"), QStringLiteral("Password"), true, 256}}},
      [&result, &completed](PromptResult value) {
        result = std::move(value);
        completed = true;
      }));
  QTRY_VERIFY(promptWindow() != nullptr);
  QQuickWindow *window = promptWindow();
  auto *identity = findItem(window->contentItem(),
                            QStringLiteral("networkSecretField-identity"));
  auto *password = findItem(window->contentItem(),
                            QStringLiteral("networkSecretField-password"));
  auto *submit =
      findItem(window->contentItem(), QStringLiteral("networkSecretSubmit"));
  QVERIFY(identity != nullptr);
  QVERIFY(password != nullptr);
  QVERIFY(submit != nullptr);
  identity->setProperty("text", QStringLiteral("etta"));
  password->setProperty("text", QStringLiteral("canary-prompt-secret"));
  QVERIFY(QMetaObject::invokeMethod(submit, "clicked"));
  QTRY_VERIFY(completed);
  QVERIFY(result.accepted);
  QCOMPARE(result.values.size(), 2);
  QCOMPARE(result.values.at(0).bytes, QByteArray("etta"));
  QCOMPARE(result.values.at(1).bytes, QByteArray("canary-prompt-secret"));
  QCOMPARE(identity->property("text").toString(), QString{});
  QCOMPARE(password->property("text").toString(), QString{});
  result.wipe();
}

QTEST_MAIN(NetworkSecretPromptTest)
#include "tst_network_secret_prompt.moc"
