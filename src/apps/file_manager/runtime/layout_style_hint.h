// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
class SettingsTransport;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Apps::FileManager {

// ADR-0271: the File Manager style the selected desktop layout asks for --
// the profile's workflow.fileManager ("finder", "explorer", "commander").
// PreferencesController uses it as the style until the user picks one.
//
// AGENT-CONTRACT: reads, and never writes, two things other modules own:
// the Settings1 key `panels.layoutProfile` (the shell and Settings Customize
// select the layout, ADR-0267) and WorkflowSpec::fileManager in the layout
// profile catalog (qindaqt/profiles/layout_profile.h, ADR-0268). The catalog
// is read once, at construction; the selection is followed live. Without a
// session bus, Settings1, or a matching profile, the hint is empty (Finder).
// GUI-thread only.
class LayoutStyleHint final : public QObject {
  Q_OBJECT

public:
  // Owns `transport`; `profileHints` maps a layout profile id to its
  // workflow.fileManager (loadProfileHints()).
  LayoutStyleHint(std::unique_ptr<Services::SettingsClient::SettingsTransport> transport,
                  QHash<QString, QString> profileHints, QObject *parent = nullptr);
  ~LayoutStyleHint() override;

  // Production: Settings1 over the session bus and the installed catalog
  // (profileDirectories()).
  [[nodiscard]] static std::unique_ptr<LayoutStyleHint> forSession();

  // The selected layout's hint; empty until Settings1 answers.
  [[nodiscard]] QString hint() const { return m_hint; }

  // Every profile in `directories` that loads, later directories overriding
  // earlier ones as the shell's catalog does; a profile that does not load
  // gives no hint.
  [[nodiscard]] static QHash<QString, QString> loadProfileHints(const QStringList &directories);
  // The shell's catalog directories, lowest precedence first:
  // $QINDAQT_PROFILE_DIR when set, else every existing qindaqt/profiles
  // under the XDG data locations, the user's own last.
  [[nodiscard]] static QStringList profileDirectories();

signals:
  void hintChanged(const QString &hint);

private:
  void update();

  // Declared in construction order: the client borrows the transport.
  std::unique_ptr<Services::SettingsClient::SettingsTransport> m_transport;
  std::unique_ptr<Services::SettingsClient::SettingsClient> m_client;
  QHash<QString, QString> m_profileHints;
  QString m_hint;
};

} // namespace QindaQt::Apps::FileManager
