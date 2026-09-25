// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>

class QProcess;
class QTimer;

namespace QindaQt::QindaLutris {

class LibraryController;
class ProcessTreeSupervisor;
class ScopedGameLauncher;
struct TreeStopOutcome;

// AGENT-CONTRACT: which of the games QindaLutris started are still running,
// and Force quit (ADR-0275 section 4b), exposed to QML as `Running`. A game
// is tracked by the systemd scope ScopedGameLauncher put it in; liveness is
// polled asynchronously (`systemctl --user is-active`, never blocking the GUI
// thread) and Force quit stops the whole scope through the jobs package's
// ProcessTreeSupervisor (TERM, grace, KILL -- on its worker thread). Games
// started unscoped (Steam/Lutris clients, or no user manager) are not
// tracked and cannot be force-quit here.
class RunningGames final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList runningIds READ runningIds NOTIFY changed)
  Q_PROPERTY(QString message READ message NOTIFY changed)
public:
  RunningGames(LibraryController *library, ScopedGameLauncher *launcher,
               QObject *parent = nullptr);
  ~RunningGames() override;

  [[nodiscard]] QStringList runningIds() const { return m_units.keys(); }
  [[nodiscard]] QString message() const { return m_message; }

  Q_INVOKABLE bool isRunning(const QString &gameId) const { return m_units.contains(gameId); }
  Q_INVOKABLE void forceQuit(const QString &gameId);

  // Pure: `systemctl is-active` prints one state per unit, in order.
  [[nodiscard]] static QStringList activeUnits(const QStringList &units,
                                               const QByteArray &isActiveOutput);

Q_SIGNALS:
  void changed();

private:
  void onLaunched(const QString &gameId);
  void poll();
  void onStopped(const QString &gameId, const TreeStopOutcome &outcome);

  LibraryController *m_library = nullptr;
  ScopedGameLauncher *m_launcher = nullptr;
  QHash<QString, QString> m_units; // game id -> scope unit
  QHash<QString, ProcessTreeSupervisor *> m_stopping;
  QTimer *m_pollTimer = nullptr;
  QProcess *m_poll = nullptr;
  QString m_message;
};

} // namespace QindaQt::QindaLutris
