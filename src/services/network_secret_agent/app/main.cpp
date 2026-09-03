// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/services/network_secret_agent/qml_prompt_presenter.h>
#include <qindaqt/services/network_secret_agent/resident_secret_agent.h>
#include <qindaqt/themes/theme_loader.h>

#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>
#include <QtDBus/QDBusConnection>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

#include <cstdio>
#include <memory>

namespace {

bool publishTokens(QQmlEngine &engine, QString *error) {
  QQmlComponent registration(&engine);
  registration.setData(
      R"qml(
    import QtQuick
    import QindaQt.Tokens 1.0
    QtObject { property int revision: Tokens.qstRevision }
  )qml",
      QUrl(QStringLiteral("inline:network-secret-agent-tokens.qml")));
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
    if (error != nullptr) {
      *error = registration.errorString();
    }
    return false;
  }
  std::unique_ptr<QObject> object(registration.create());
  auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
      QStringLiteral("QindaQt.Tokens"), QStringLiteral("Tokens"));
  const QFileInfo executable(QCoreApplication::applicationFilePath());
  const bool buildExecutable =
      executable.canonicalFilePath() ==
      QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH))
          .canonicalFilePath();
  const QString themePath =
      buildExecutable
          ? QStringLiteral(QINDAQT_BUILD_THEME_PATH)
          : QDir(executable.absolutePath())
                .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_THEME_PATH));
  const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(themePath);
  if (object == nullptr || facade == nullptr || !loaded.ok) {
    if (error != nullptr) {
      *error = loaded.ok ? QStringLiteral("token singleton unavailable")
                         : loaded.error;
    }
    return false;
  }
  return facade->publish(loaded.theme, {}, error);
}

void bindBusLifetime(QDBusConnection connection, QObject &receiver) {
  (void)connection.connect({}, QStringLiteral("/org/freedesktop/DBus/Local"),
                           QStringLiteral("org.freedesktop.DBus.Local"),
                           QStringLiteral("Disconnected"), &receiver,
                           SLOT(quit()));
}

} // namespace

int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  application.setApplicationName(
      QStringLiteral("qindaqt-network-secret-agent"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(
      QStringLiteral("org.qindaqt.NetworkSecretAgent"));

  QQmlEngine engine;
  const QFileInfo executable(QCoreApplication::applicationFilePath());
  if (executable.canonicalFilePath() ==
      QFileInfo(QStringLiteral(QINDAQT_BUILD_EXECUTABLE_PATH))
          .canonicalFilePath()) {
    engine.addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
  }
  engine.addImportPath(
      QDir(executable.absolutePath())
          .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_PATH)));
  QString tokenError;
  if (!publishTokens(engine, &tokenError)) {
    std::fprintf(stderr, "qindaqt-network-secret-agent: UI setup failed\n");
    return 3;
  }
  if (application.arguments().contains(QStringLiteral("--check-package"))) {
    return 0;
  }

  QDBusConnection systemConnection = QDBusConnection::systemBus();
  QDBusConnection presenceConnection = QDBusConnection::sessionBus();
  bindBusLifetime(systemConnection, application);
  bindBusLifetime(presenceConnection, application);
  QindaQt::Network::SecretAgent::QmlPromptPresenter prompt(engine);
  QindaQt::Network::SecretAgent::ResidentSecretAgent agent(
      prompt, systemConnection, presenceConnection);
  if (agent.start() !=
      QindaQt::Network::SecretAgent::ResidentStartStatus::Started) {
    std::fprintf(stderr, "qindaqt-network-secret-agent: startup failed\n");
    return 1;
  }
  QObject::connect(&application, &QCoreApplication::aboutToQuit, &agent,
                   &QindaQt::Network::SecretAgent::ResidentSecretAgent::stop);
  return QGuiApplication::exec();
}
