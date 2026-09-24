// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>

#include <memory>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
class SettingsTransport;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Services::DockItems {
class Settings1DockPins;
} // namespace QindaQt::Services::DockItems

namespace QindaQt::Apps::FileManager {

// Keep in Dock for the Applications place (ADR-0273): one window's face of
// the dock's public pin helper, Settings1DockPins (ADR-0265). It keeps the one
// selected application in the dock, or removes it. The shell's dock adopts
// the new value from Settings1 like any other writer's; this process never
// talks to the shell.
//
// AGENT-CONTRACT: ui/ApplicationsPlaceActions.qml writes `applicationId` (the
// one selected Applications row, or empty) and runs toggle() for the
// application.keep-in-dock action; app_shell/file_manager_dock_actions reads
// `available` and `pinned` for that action's enabled and checked state.
// `pinned` is confirmed truth only: a write Settings1 refuses, or cannot
// confirm, leaves it unchanged. GUI-thread only.
class ApplicationDockPins final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString applicationId READ applicationId WRITE setApplicationId NOTIFY
                 stateChanged FINAL)
  Q_PROPERTY(bool available READ available NOTIFY stateChanged FINAL)
  Q_PROPERTY(bool pinned READ pinned NOTIFY stateChanged FINAL)

public:
  // Owns `transport` and starts a Settings1 client over it scoped to the
  // dock's keys. Until Settings1 answers, or without it, nothing is
  // available.
  explicit ApplicationDockPins(
      std::unique_ptr<Services::SettingsClient::SettingsTransport> transport,
      QObject *parent = nullptr);
  ~ApplicationDockPins() override;

  [[nodiscard]] QString applicationId() const { return m_applicationId; }
  void setApplicationId(const QString &applicationId);
  // An application is chosen, the confirmed dock is loaded, and no change is
  // waiting for Settings1.
  [[nodiscard]] bool available() const;
  // The chosen application is in the confirmed dock, in a group or not.
  [[nodiscard]] bool pinned() const;
  // Keeps the chosen application in the dock, or removes it. False when
  // nothing is available; true means Settings1 was asked, and `pinned`
  // changes once it confirms.
  Q_INVOKABLE bool toggle();

signals:
  void stateChanged();

private:
  // Declared in construction order: each member borrows the one before it.
  std::unique_ptr<Services::SettingsClient::SettingsTransport> m_transport;
  std::unique_ptr<Services::SettingsClient::SettingsClient> m_client;
  std::unique_ptr<Services::DockItems::Settings1DockPins> m_pins;
  QString m_applicationId;
};

} // namespace QindaQt::Apps::FileManager
