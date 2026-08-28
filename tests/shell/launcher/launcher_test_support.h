// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/application_catalog.h"
#include "qindaqt/shell_launcher/launcher_types.h"

#include <QString>
#include <QVector>

// Deterministic desktop-entry fixtures. AGENT-GUARD: Every fixture is literal
// text here; no test may read the host filesystem, environment, or menus. The
// hostile corpus lives next to these helpers so every test file sees the same
// malformed/duplicate/hidden documents.

namespace QindaQt::ShellLauncher::TestSupport {

struct EntryTemplate {
  QString type = QStringLiteral("Application");
  QString name = QStringLiteral("Fixture App");
  QString genericName;
  QString comment;
  QString icon = QStringLiteral("applications-utilities");
  QString noDisplayLine;
  QString hiddenLine;
  QString categories = QStringLiteral("Utility");
  QString keywords = QStringLiteral("fixture;demo");
  QString actionsValue;
  QString actionGroups;
  QString extraBody;

  QString toText() const
  {
    QString text = QStringLiteral("[Desktop Entry]\nType=%1\nName=%2\n")
                       .arg(type, name);
    if (!genericName.isEmpty())
      text += QStringLiteral("GenericName=%1\n").arg(genericName);
    if (!comment.isEmpty())
      text += QStringLiteral("Comment=%1\n").arg(comment);
    text += QStringLiteral("Icon=%1\n").arg(icon);
    if (!noDisplayLine.isEmpty())
      text += noDisplayLine + QLatin1Char('\n');
    if (!hiddenLine.isEmpty())
      text += hiddenLine + QLatin1Char('\n');
    text += QStringLiteral("Categories=%1\nKeywords=%2\n").arg(categories, keywords);
    if (!actionsValue.isEmpty())
      text += QStringLiteral("Actions=%1\n").arg(actionsValue);
    if (!extraBody.isEmpty())
      text += extraBody + QLatin1Char('\n');
    if (!actionGroups.isEmpty())
      text += actionGroups;
    return text;
  }
};

inline QString actionGroup(const QString &id, const QString &name,
                           const QString &icon = {})
{
  QString text = QStringLiteral("\n[Desktop Action %1]\nName=%2\n").arg(id, name);
  if (!icon.isEmpty())
    text += QStringLiteral("Icon=%1\n").arg(icon);
  return text;
}

inline SourceDocument document(const QString &id, const QString &text)
{
  return SourceDocument { id, text };
}

inline SourceDocument document(const QString &id, const EntryTemplate &entry)
{
  return SourceDocument { id, entry.toText() };
}

inline QVector<SourceDocument> catalogCorpus()
{
  EntryTemplate editor;
  editor.name = QStringLiteral("Qinda Editor");
  editor.genericName = QStringLiteral("Text Editor");
  editor.comment = QStringLiteral("Write and edit documents");
  editor.icon = QStringLiteral("accessories-text-editor");
  editor.categories = QStringLiteral("Office;WordProcessor;");
  editor.keywords = QStringLiteral("write;text;");
  editor.actionsValue = QStringLiteral("new-window;");
  editor.actionGroups = actionGroup(QStringLiteral("new-window"),
                                    QStringLiteral("New Window"));

  EntryTemplate terminal;
  terminal.name = QStringLiteral("Qinda Terminal");
  terminal.genericName = QStringLiteral("Terminal Emulator");
  terminal.comment = QStringLiteral("Use the command line");
  terminal.icon = QStringLiteral("utilities-terminal");
  terminal.categories = QStringLiteral("System;Utility;");

  EntryTemplate viewer;
  viewer.name = QStringLiteral("Image Viewer");
  viewer.comment = QStringLiteral("Browse pictures");
  viewer.icon = QStringLiteral("image-viewer");
  viewer.categories = QStringLiteral("Graphics;");

  EntryTemplate music;
  music.name = QStringLiteral("Music Player");
  music.comment = QStringLiteral("Play audio files");
  music.icon = QStringLiteral("audio-player");
  music.categories = QStringLiteral("AudioVideo;Music;");

  EntryTemplate browser;
  browser.name = QStringLiteral("Web Browser");
  browser.comment = QStringLiteral("Browse the web");
  browser.icon = QStringLiteral("web-browser");
  browser.categories = QStringLiteral("Network;");
  browser.keywords = QStringLiteral("internet;");

  return {
    document(QStringLiteral("org.qinda.editor"), editor),
    document(QStringLiteral("org.qinda.terminal"), terminal),
    document(QStringLiteral("org.qinda.viewer"), viewer),
    document(QStringLiteral("org.qinda.music"), music),
    document(QStringLiteral("org.qinda.browser"), browser),
  };
}

} // namespace QindaQt::ShellLauncher::TestSupport
