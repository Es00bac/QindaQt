// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/services/network_secret_agent/qml_prompt_presenter.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
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
