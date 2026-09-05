// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_pinned_recent.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace QindaQt::Shell::Launcher {

class ApplicationScanner;
class LaunchExecutor;
class LauncherPersistenceController;

// Shell-private adapter from the launcher adapters to bounded values for the
// compiled QindaQt.Shell.Launcher QML module. The QML never sees a catalog,
// document, transport, or process object — only this controller's value
// projection, mirroring the power applet's controller boundary.
//
// AGENT-CONTRACT: All borrowed collaborators may be null (preview and
// degraded composition); every property then reports its empty/unavailable
// truth and every invocable fails closed. The controller never starts or
// stops collaborators; the composition root owns their lifetimes.
// applicationsLaunchGranted comes from the audited manifest/policy evaluation
// and gates activation; without it launch requests are refused before
// reaching the executor.
class LauncherAppletController final : public QObject
{
  Q_OBJECT
  // loading | ready | empty | degraded | unavailable
  Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
  Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY stateChanged)
  Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
  Q_PROPERTY(QVariantList sections READ sections NOTIFY stateChanged)
  Q_PROPERTY(bool launchGranted READ launchGranted NOTIFY stateChanged)
  Q_PROPERTY(QString persistenceStatus READ persistenceStatus NOTIFY stateChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  LauncherAppletController(ApplicationScanner *scanner,
                           LauncherPersistenceController *persistence,
                           LaunchExecutor *executor,
                           bool applicationsLaunchGranted,
                           QObject *parent = nullptr);

  [[nodiscard]] QString phase() const;
  [[nodiscard]] QString diagnostic() const;
  [[nodiscard]] QString query() const { return m_query; }
  void setQuery(const QString &query);
  [[nodiscard]] QVariantList sections() const { return m_sections; }
  // Pure projection of the same catalog/pinned/recent truth for an arbitrary
  // query, in the same section/item shape as `sections`. It never touches
  // `query` or `sections`, so other shell controls (quick launch, command
  // search) can read the launcher without disturbing the launcher popup.
  Q_INVOKABLE [[nodiscard]] QVariantList sectionsForQuery(const QString &query) const;
  [[nodiscard]] bool launchGranted() const noexcept { return m_launchGranted; }
  [[nodiscard]] QString persistenceStatus() const;
  [[nodiscard]] QString feedback() const { return m_feedback; }

  // Activation resolves through the catalog intent builder inside the
  // executor; returns false without side effects when the grant is missing,
  // the executor is absent, or the launch is refused/failed.
  Q_INVOKABLE bool activate(const QString &entryId, const QString &actionId = {});
  Q_INVOKABLE bool pin(const QString &entryId);
  Q_INVOKABLE bool unpin(const QString &entryId);
  Q_INVOKABLE bool movePinnedUp(const QString &entryId);
  Q_INVOKABLE bool movePinnedDown(const QString &entryId);
  Q_INVOKABLE bool clearRecent();
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void stateChanged();
  void queryChanged();
  void feedbackChanged();

private:
  void rebuild();
  void publishFeedback(const QString &message);

  ApplicationScanner *m_scanner = nullptr;
  LauncherPersistenceController *m_persistence = nullptr;
  LaunchExecutor *m_executor = nullptr;
  bool m_launchGranted = false;
  QString m_query;
  QVariantList m_sections;
  QString m_feedback;
  QindaQt::ShellLauncher::PinnedApplications m_fallbackPinned;
  QindaQt::ShellLauncher::RecentApplications m_fallbackRecent;
};

} // namespace QindaQt::Shell::Launcher
