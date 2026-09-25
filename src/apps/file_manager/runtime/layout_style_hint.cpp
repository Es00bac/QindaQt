// SPDX-License-Identifier: GPL-3.0-or-later
#include "layout_style_hint.h"

#include "qindaqt/profiles/profile_loader.h"

#include <qindaqt/services/settings_client/qt_settings_transport.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>

#include <QDBusConnection>
#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Apps::FileManager {
namespace {

// AGENT-CONTRACT: the shell's and Settings Customize's key for the selected
// layout (shellpreferencevalues.cpp, customize_settings_model.h).
const QString kLayoutProfileKey = QStringLiteral("panels.layoutProfile");

} // namespace

using Services::SettingsClient::SettingsClient;
using Services::SettingsClient::SettingsTransport;

LayoutStyleHint::LayoutStyleHint(std::unique_ptr<SettingsTransport> transport,
                                 QHash<QString, QString> profileHints, QObject *parent)
    : QObject(parent), m_transport(std::move(transport)),
      m_client(std::make_unique<SettingsClient>(*m_transport, QStringList{kLayoutProfileKey})),
      m_profileHints(std::move(profileHints)) {
  connect(m_client.get(), &SettingsClient::snapshotChanged, this, &LayoutStyleHint::update);
  // Without a session bus or Settings1 the client stays unavailable and the
  // hint stays empty: the File Manager is Finder-style, as before.
  const bool started = m_client->start();
  Q_UNUSED(started);
}

LayoutStyleHint::~LayoutStyleHint() {
  // Stop before the transport the client reads goes away.
  m_client->stop();
}

std::unique_ptr<LayoutStyleHint> LayoutStyleHint::forSession() {
  return std::make_unique<LayoutStyleHint>(
      std::make_unique<Services::SettingsClient::QtSettingsTransport>(
          QDBusConnection::sessionBus()),
      loadProfileHints(profileDirectories()));
}

void LayoutStyleHint::update() {
  const auto &snapshot = m_client->snapshot();
  const QString profile =
      snapshot ? snapshot->values.value(kLayoutProfileKey).toString() : QString();
  const QString hint = m_profileHints.value(profile);
  if (hint == m_hint) {
    return;
  }
  m_hint = hint;
  emit hintChanged(m_hint);
}

QHash<QString, QString> LayoutStyleHint::loadProfileHints(const QStringList &directories) {
  QHash<QString, QString> hints;
  for (const QString &directory : directories) {
    for (const auto &loaded : Profiles::ProfileLoader::fromDirectory(directory)) {
      if (loaded.ok) {
        hints.insert(loaded.profile.id, loaded.profile.workflow.fileManager);
      }
    }
  }
  return hints;
}

QStringList LayoutStyleHint::profileDirectories() {
  const QString override = qEnvironmentVariable("QINDAQT_PROFILE_DIR");
  if (!override.isEmpty()) {
    return {QDir::cleanPath(override)};
  }
  QStringList directories;
  const QStringList roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  for (auto root = roots.crbegin(); root != roots.crend(); ++root) {
    const QString path = QDir(*root).filePath(QStringLiteral("qindaqt/profiles"));
    if (QDir(path).exists() && !directories.contains(path)) {
      directories.append(path);
    }
  }
  return directories;
}

} // namespace QindaQt::Apps::FileManager
