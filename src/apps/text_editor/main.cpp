// SPDX-License-Identifier: GPL-3.0-or-later
#include "document/local_document_store.h"
#include "ui/editor_appearance.h"
#include "ui/editor_window.h"

#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/themes/theme_loader.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cstdio>
#include <memory>

namespace {

[[nodiscard]] QStringList
themeSearchDirectories(const QString &explicitDirectory) {
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
    return {.ok = false,
            .theme = {},
            .error = QStringLiteral("Invalid theme identifier")};
  }
  for (const QString &directory : directories) {
    const QString path =
        QDir(directory).filePath(themeId + QStringLiteral(".json"));
    if (QFileInfo::exists(path)) {
      return QindaQt::Themes::ThemeLoader::fromFile(path);
    }
  }
  return {.ok = false,
          .theme = {},
          .error = QStringLiteral("Theme '%1' was not found").arg(themeId)};
}

} // namespace

int main(int argc, char **argv) {
  QElapsedTimer startupTimer;
  startupTimer.start();
  QApplication application(argc, argv);
  application.setApplicationName(QStringLiteral("qindaqt-editor"));
  application.setApplicationDisplayName(QStringLiteral("QindaQt Text Editor"));
  application.setOrganizationName(QStringLiteral("QindaQt"));
  application.setDesktopFileName(QStringLiteral("org.qindaqt.TextEditor"));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("QindaQt UTF-8 text editor"));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption({QStringLiteral("theme"),
                    QStringLiteral("QindaQt theme identifier"),
                    QStringLiteral("id"), QStringLiteral("qinda-dark")});
  parser.addOption({QStringLiteral("theme-directory"),
                    QStringLiteral("Additional local theme directory"),
                    QStringLiteral("path")});
  parser.addOption(
      {QStringLiteral("check-theme"),
       QStringLiteral("Validate the selected theme through QST-1 and exit")});
  parser.addOption(
      {QStringLiteral("report-startup"),
       QStringLiteral("Print milliseconds to the first painted frame")});
  parser.addPositionalArgument(QStringLiteral("file"),
                               QStringLiteral("Local UTF-8 file to open"),
                               QStringLiteral("[file]"));
  parser.process(application);
  if (parser.positionalArguments().size() > 1) {
    std::fprintf(stderr, "qindaqt-editor: open one document at a time\n");
    return 2;
  }

  const auto theme = loadTheme(
      parser.value(QStringLiteral("theme")),
      themeSearchDirectories(parser.value(QStringLiteral("theme-directory"))));
  if (!theme.ok) {
    std::fprintf(stderr, "qindaqt-editor: %s\n", qPrintable(theme.error));
    return 3;
  }
  const auto appearance =
      QindaQt::Apps::TextEditor::EditorAppearanceAdapter::fromTheme(
          theme.theme);
  if (!appearance.ok()) {
    std::fprintf(stderr, "qindaqt-editor: %s\n",
                 qPrintable(appearance.diagnostic));
    return 3;
  }
  application.setPalette(appearance.appearance->palette);
  application.setFont(appearance.appearance->interfaceFont);
  if (parser.isSet(QStringLiteral("check-theme"))) {
    std::printf("%s qst-%d\n", qPrintable(appearance.appearance->sourceThemeId),
                QindaQt::DesignTokens::DesignTokens::qstRevision);
    return 0;
  }

  QindaQt::Apps::TextEditor::EditorWindow window(
      std::make_unique<QindaQt::Apps::TextEditor::LocalDocumentStore>(),
      *appearance.appearance);
  if (parser.isSet(QStringLiteral("report-startup"))) {
    QObject::connect(
        &window, &QindaQt::Apps::TextEditor::EditorWindow::firstFramePainted,
        &window,
        [&startupTimer] {
          std::printf("startup-first-frame-ms=%lld\n",
                      static_cast<long long>(startupTimer.elapsed()));
          std::fflush(stdout);
        },
        Qt::SingleShotConnection);
  }
  if (!parser.positionalArguments().isEmpty()) {
    const auto result =
        window.controller()->openPath(parser.positionalArguments().first());
    if (!result.ok()) {
      std::fprintf(stderr, "qindaqt-editor: %s\n",
                   qPrintable(result.diagnostic));
      return 4;
    }
  }
  window.show();
  return application.exec();
}
