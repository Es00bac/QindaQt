// SPDX-License-Identifier: GPL-3.0-or-later
#include "model/launch_intent.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVariant>

#include <cstdio>
#include <memory>

namespace {

[[nodiscard]] QStringList themeSearchDirectories(const QString &explicitDirectory) {
  QStringList directories;
  if (!explicitDirectory.isEmpty()) {
    directories.append(QFileInfo(explicitDirectory).absoluteFilePath());
  }
  directories.append(QStandardPaths::locateAll(
      QStandardPaths::GenericDataLocation, QStringLiteral("qindaqt/themes"),
      QStandardPaths::LocateDirectory));
  directories.append(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(QStringLiteral("../share/qindaqt/themes")));
  directories.removeDuplicates();
  return directories;
}

[[nodiscard]] QindaQt::Themes::LoadResult
loadTheme(const QString &themeId, const QStringList &directories) {
  static const QRegularExpression safeId(
      QStringLiteral("^[a-z0-9][a-z0-9-]{0,63}$"));
  if (!safeId.match(themeId).hasMatch()) {
    return {.ok = false, .theme = {}, .error = QStringLiteral("Invalid theme identifier")};
  }
  for (const QString &directory : directories) {
    const QString path = QDir(directory).filePath(themeId + QStringLiteral(".json"));
    if (QFileInfo::exists(path)) {
      return QindaQt::Themes::ThemeLoader::fromFile(path);
    }
  }
  return {.ok = false, .theme = {},
          .error = QStringLiteral("Theme '%1' was not found").arg(themeId)};
}

// AGENT-CONTRACT: A QML engine must import QindaQt.Tokens 1.0 through one
// component before engine.singletonInstance() resolves the generated plugin's
// registration, and TokenFacade::publish() must complete before the real root
// QML is created so every Controls binding reads a complete generation on its
// first evaluation. See tests/controls/control_test_support.cpp for the same
// sequence and src/design_tokens/include/qindaqt/design_tokens/token_facade.h
// for the GUI-thread/publish-before-construct contract.
[[nodiscard]] QindaQt::DesignTokens::TokenFacade *
registerAndPublishTokens(QQmlApplicationEngine &engine,
                         const QindaQt::Themes::ThemeSpec &theme,
                         QString *error) {
  QQmlComponent registration(&engine);
  registration.setData(R"qml(
      import QtQuick
      import QindaQt.Tokens 1.0
      QtObject { property int revision: Tokens.qstRevision }
  )qml",
                       QUrl(QStringLiteral("inline:qindaqt-file-manager-token-registration.qml")));
  if (registration.status() == QQmlComponent::Error) {
    if (error) {
      *error = registration.errorString();
    }
    return nullptr;
  }
  std::unique_ptr<QObject> registrationObject(registration.create());
  if (!registrationObject) {
    if (error) {
      *error = registration.errorString();
    }
    return nullptr;
  }
  auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
      "QindaQt.Tokens", "Tokens");
  if (!facade) {
    if (error) {
      *error = QStringLiteral("QindaQt.Tokens singleton was not registered");
    }
    return nullptr;
  }
  if (!facade->publish(theme, {}, error)) {
    return nullptr;
  }
  return facade;
}

} // namespace

int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-file-manager"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt File Manager"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.FileManager"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt local file manager"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({QStringLiteral("theme"), QStringLiteral("QindaQt theme identifier"),
                    QStringLiteral("id"), QStringLiteral("qinda-dark")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Additional local theme directory"), QStringLiteral("path")});
  parser.addOption({QStringLiteral("check-theme"),
                    QStringLiteral("Validate the selected theme through QST-1 and exit")});
  parser.addOption(
      {QStringLiteral("check-qml-root"),
       QStringLiteral("Construct the QML root for an installed-package probe and exit")});
  parser.addPositionalArgument(QStringLiteral("folder"),
                               QStringLiteral("Local folder to open"), QStringLiteral("[folder]"));
  parser.process(application);
  if (parser.positionalArguments().size() > 1) {
    std::fprintf(stderr, "qindaqt-file-manager: open one folder at a time\n");
    return 2;
  }

  const auto theme = loadTheme(parser.value(QStringLiteral("theme")),
                               themeSearchDirectories(parser.value(QStringLiteral("theme-directory"))));
  if (!theme.ok) {
    std::fprintf(stderr, "qindaqt-file-manager: %s\n", qPrintable(theme.error));
    return 3;
  }
  if (parser.isSet(QStringLiteral("check-theme"))) {
    std::printf("%s qst-%d\n", qPrintable(theme.theme.id),
                QindaQt::DesignTokens::DesignTokens::qstRevision);
    return 0;
  }

  QString startPath = QDir::homePath();
  if (!parser.positionalArguments().isEmpty()) {
    const QFileInfo requested(parser.positionalArguments().first());
    if (requested.isDir()) {
      startPath = requested.absoluteFilePath();
    } else {
      std::fprintf(stderr, "qindaqt-file-manager: %s is not a folder\n",
                   qPrintable(parser.positionalArguments().first()));
      return 4;
    }
  }

  QQmlApplicationEngine engine;
  // AGENT-GUARD: Resolve the private package prefix from the installed
  // executable. An absolute build-tree import here makes package probes pass
  // on a developer machine while shipped clients fail to load Tokens/Controls.
  engine.addImportPath(
      QDir(QCoreApplication::applicationDirPath())
          .absoluteFilePath(QStringLiteral(QINDAQT_INSTALL_QML_RELATIVE_PATH)));
  QString tokenError;
  if (!registerAndPublishTokens(engine, theme.theme, &tokenError)) {
    std::fprintf(stderr, "qindaqt-file-manager: %s\n", qPrintable(tokenError));
    return 3;
  }

  auto controller = std::make_unique<QindaQt::Apps::FileManager::NavigationController>(
      std::make_unique<QindaQt::Apps::FileManager::LocalDirectoryLister>(),
      std::make_unique<QindaQt::Apps::FileManager::DesktopFileLauncher>());
  controller->navigateTo(startPath);

  engine.setInitialProperties(
      {{QStringLiteral("navigationController"),
        QVariant::fromValue(static_cast<QObject *>(controller.get()))}});
  engine.loadFromModule(QStringLiteral("QindaQt.FileManagerApp"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty()) {
    return 3;
  }
  if (parser.isSet(QStringLiteral("check-qml-root"))) {
    std::printf("qml-root-loaded\n");
    return 0;
  }
  return application.exec();
}
