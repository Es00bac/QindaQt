// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>

#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>

#include <QFileInfo>
#include <QStandardPaths>

#include <optional>
#include <utility>

using QindaQt::Session::DesktopControls::ScreensaverCatalogEntry;
using QindaQt::Session::DesktopControls::ScreensaverCatalog;
using QindaQt::Session::DesktopControls::ScreensaverPreferences;

namespace QindaQt::Apps::SettingsScreensaver {

ProcessScreensaverPreview::ProcessScreensaverPreview(
    const ScreensaverCatalog &catalog, QObject *parent)
    : ScreensaverPreview(parent), m_catalog(catalog),
      m_greeterPath(resolveGreeter()) {}

ProcessScreensaverPreview::ProcessScreensaverPreview(
    const ScreensaverCatalog &catalog, QString greeterPath, QObject *parent)
    : ScreensaverPreview(parent), m_catalog(catalog),
      m_greeterPath(std::move(greeterPath)) {
  // A test that wants the "no greeter installed" path passes an empty path.
}

ProcessScreensaverPreview::~ProcessScreensaverPreview() {
  if (m_process.state() != QProcess::NotRunning) {
    m_process.kill();
    m_process.waitForFinished(500);
  }
}

QStringList ProcessScreensaverPreview::greeterCandidates() {
  QStringList candidates;
  const QString onPath =
      QStandardPaths::findExecutable(QStringLiteral("kscreenlocker_greet"));
  if (!onPath.isEmpty()) {
    candidates.append(onPath);
  }
  candidates.append({
      QStringLiteral("/usr/libexec/kscreenlocker_greet"),
      QStringLiteral("/usr/lib/libexec/kscreenlocker_greet"),
      QStringLiteral("/usr/lib64/libexec/kscreenlocker_greet"),
      QStringLiteral("/usr/local/libexec/kscreenlocker_greet"),
  });
  return candidates;
}

QString ProcessScreensaverPreview::resolveGreeter() {
  const QStringList candidates = greeterCandidates();
  for (const QString &candidate : candidates) {
    if (QFileInfo::exists(candidate) && QFileInfo(candidate).isExecutable()) {
      return candidate;
    }
  }
  return {};
}

QString ProcessScreensaverPreview::greeter() const { return m_greeterPath; }

ScreensaverPreview::Kind
ProcessScreensaverPreview::kindFor(const QString &token) const {
  if (token.isEmpty() || token == ScreensaverPreferences::noneToken()) {
    return Kind::Unavailable;
  }
  if (token == ScreensaverPreferences::blankToken()) {
    // Blank has no program; the preview is the dark ground the locker draws.
    return greeter().isEmpty() ? Kind::Unavailable : Kind::TestingGreeter;
  }
  const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(token);
  if (!entry.has_value()) {
    return Kind::Unavailable;
  }
  if (entry->showsOnLockScreen) {
    return greeter().isEmpty() ? Kind::Unavailable : Kind::TestingGreeter;
  }
  return Kind::SaverProgram;
}

bool ProcessScreensaverPreview::running() const {
  return m_process.state() != QProcess::NotRunning;
}

bool ProcessScreensaverPreview::start(const QString &token, QString *error) {
  if (running()) {
    if (error != nullptr) {
      *error = tr("A preview is already open.");
    }
    return false;
  }
  const Kind kind = kindFor(token);
  QString program;
  QStringList arguments;
  if (kind == Kind::TestingGreeter) {
    program = greeter();
    arguments = {QStringLiteral("--testing")};
  } else if (kind == Kind::SaverProgram) {
    const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(token);
    if (!entry.has_value()) {
      if (error != nullptr) {
        *error = tr("That screensaver is not available to preview.");
      }
      return false;
    }
    program = entry->token;
    arguments = entry->arguments;
  } else {
    if (error != nullptr) {
      *error = tr("There is nothing to preview for this choice.");
    }
    return false;
  }

  connect(&m_process, &QProcess::finished, this, &ScreensaverPreview::finished,
          Qt::UniqueConnection);
  m_process.start(program, arguments);
  if (!m_process.waitForStarted(3000)) {
    if (error != nullptr) {
      *error = kind == Kind::TestingGreeter
          ? tr("The lock screen preview could not start: %1")
                .arg(m_process.errorString())
          : tr("The screensaver could not start: %1")
                .arg(m_process.errorString());
    }
    return false;
  }
  return true;
}

} // namespace QindaQt::Apps::SettingsScreensaver
