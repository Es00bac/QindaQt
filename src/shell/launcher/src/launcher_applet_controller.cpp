// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launcher_applet_controller.h"

#include "application_scanner.h"
#include "launch_executor.h"
#include "launcher_persistence.h"

#include "qindaqt/shell_launcher/launcher_bounds.h"
#include "qindaqt/shell_launcher/launcher_category_model.h"
#include "qindaqt/shell_launcher/launcher_presentation.h"

#include <QVariantMap>

namespace QindaQt::Shell::Launcher {

using QindaQt::ShellLauncher::LauncherPresentation;
using QindaQt::ShellLauncher::LauncherPresentationModel;
using QindaQt::ShellLauncher::LauncherStatus;
using QindaQt::ShellLauncher::PinnedApplications;
using QindaQt::ShellLauncher::PresentationSection;
using QindaQt::ShellLauncher::RecentApplications;
using QindaQt::ShellLauncher::SectionKind;
using QindaQt::ShellLauncher::SectionLabel;

namespace {

// Stable, locale-independent section identities; the QML adapter translates
// them (the L0 model deliberately owns no user-facing strings).
QString sectionIdentity(const PresentationSection &section)
{
  switch (section.label) {
  case SectionLabel::Pinned:
    return QStringLiteral("pinned");
  case SectionLabel::Recent:
    return QStringLiteral("recent");
  case SectionLabel::SearchResults:
    return QStringLiteral("searchResults");
  case SectionLabel::Category:
    break;
  }
  switch (section.category) {
  case QindaQt::ShellLauncher::LauncherCategory::Utilities:
    return QStringLiteral("utilities");
  case QindaQt::ShellLauncher::LauncherCategory::Development:
    return QStringLiteral("development");
  case QindaQt::ShellLauncher::LauncherCategory::Education:
    return QStringLiteral("education");
  case QindaQt::ShellLauncher::LauncherCategory::Games:
    return QStringLiteral("games");
  case QindaQt::ShellLauncher::LauncherCategory::Graphics:
    return QStringLiteral("graphics");
  case QindaQt::ShellLauncher::LauncherCategory::AudioVideo:
    return QStringLiteral("audioVideo");
  case QindaQt::ShellLauncher::LauncherCategory::Network:
    return QStringLiteral("network");
  case QindaQt::ShellLauncher::LauncherCategory::Office:
    return QStringLiteral("office");
  case QindaQt::ShellLauncher::LauncherCategory::Science:
    return QStringLiteral("science");
  case QindaQt::ShellLauncher::LauncherCategory::Settings:
    return QStringLiteral("settings");
  case QindaQt::ShellLauncher::LauncherCategory::System:
    return QStringLiteral("system");
  case QindaQt::ShellLauncher::LauncherCategory::Other:
    return QStringLiteral("other");
  }
  return QStringLiteral("other");
}

QVariantList projectSections(const LauncherPresentation &presentation)
{
  QVariantList sections;
  for (const PresentationSection &section : presentation.sections) {
    QVariantList items;
    for (const auto &item : section.items) {
      items.append(QVariantMap {
          { QStringLiteral("entryId"), item.entryId },
          { QStringLiteral("displayText"), item.displayText },
          { QStringLiteral("iconName"), item.iconName },
          { QStringLiteral("accessibleDescription"), item.accessibleDescription },
          { QStringLiteral("pinned"), item.pinned },
      });
    }
    sections.append(QVariantMap {
        { QStringLiteral("kind"),
          section.kind == SectionKind::Pinned ? QStringLiteral("pinned")
          : section.kind == SectionKind::Recent ? QStringLiteral("recent")
          : section.kind == SectionKind::SearchResults
              ? QStringLiteral("searchResults")
              : QStringLiteral("category") },
        { QStringLiteral("identity"), sectionIdentity(section) },
        { QStringLiteral("items"), items },
    });
  }
  return sections;
}

} // namespace

LauncherAppletController::LauncherAppletController(
    ApplicationScanner *scanner, LauncherPersistenceController *persistence,
    LaunchExecutor *executor, bool applicationsLaunchGranted, QObject *parent)
    : QObject(parent)
    , m_scanner(scanner)
    , m_persistence(persistence)
    , m_executor(executor)
    , m_launchGranted(applicationsLaunchGranted)
{
  if (m_scanner != nullptr) {
    connect(m_scanner, &ApplicationScanner::catalogChanged,
            this, [this](quint64) { rebuild(); });
  }
  if (m_persistence != nullptr) {
    connect(m_persistence, &LauncherPersistenceController::pinnedChanged,
            this, [this] { rebuild(); });
    connect(m_persistence, &LauncherPersistenceController::recentChanged,
            this, [this] { rebuild(); });
    connect(m_persistence, &LauncherPersistenceController::stateChanged,
            this, [this] { Q_EMIT stateChanged(); });
  }
  if (m_executor != nullptr) {
    connect(m_executor, &LaunchExecutor::launchFinished,
            this, [this](const LaunchOutcome &outcome) {
              if (!outcome.ok())
                publishFeedback(outcome.diagnostic);
            });
    connect(m_executor, &LaunchExecutor::activationFinished,
            this, [this](const QString &, bool ok, const QString &diagnostic) {
              if (!ok)
                publishFeedback(diagnostic);
            });
  }
  rebuild();
}

QString LauncherAppletController::phase() const
{
  if (m_scanner == nullptr || !m_scanner->started())
    return QStringLiteral("unavailable");
  const auto &catalog = m_scanner->catalog();
  if (!catalog)
    return QStringLiteral("loading");
  const bool degraded = !catalog->diagnostics().empty()
      || !m_scanner->scanDiagnostics().empty();
  if (catalog->entries().isEmpty())
    return degraded ? QStringLiteral("degraded") : QStringLiteral("empty");
  return degraded ? QStringLiteral("degraded") : QStringLiteral("ready");
}

QString LauncherAppletController::diagnostic() const
{
  if (m_scanner == nullptr || !m_scanner->started())
    return QStringLiteral("Application scanning is unavailable");
  const auto &catalog = m_scanner->catalog();
  if (!catalog)
    return {};
  // One skipped desktop file must not read as a broken launcher when other
  // applications are usable. Detailed per-source diagnostics remain in the
  // scanner/catalog and the qindaqt.launcher.scan debug category.
  if (!catalog->entries().isEmpty())
    return {};
  if (!m_scanner->scanDiagnostics().isEmpty())
    return QStringLiteral("Applications could not be loaded: %1")
        .arg(m_scanner->scanDiagnostics().constFirst().message);
  if (!catalog->diagnostics().isEmpty())
    return QStringLiteral("No usable applications were found. Check the installed application shortcuts.");
  return {};
}

QString LauncherAppletController::persistenceStatus() const
{
  if (m_persistence == nullptr)
    return QStringLiteral("Persistence is unavailable");
  return m_persistence->statusText();
}

void LauncherAppletController::setQuery(const QString &query)
{
  const QString bounded = query.left(QindaQt::ShellLauncher::Bounds::maxQueryLength);
  if (m_query == bounded)
    return;
  m_query = bounded;
  rebuild();
  Q_EMIT queryChanged();
}

void LauncherAppletController::rebuild()
{
  std::optional<QindaQt::ShellLauncher::ApplicationCatalog> catalog;
  if (m_scanner != nullptr && m_scanner->catalog())
    catalog = *m_scanner->catalog();
  const PinnedApplications &pinned =
      m_persistence != nullptr ? m_persistence->pinned() : m_fallbackPinned;
  const RecentApplications &recent =
      m_persistence != nullptr ? m_persistence->recent() : m_fallbackRecent;

  m_sections = projectSections(
      LauncherPresentationModel::build(catalog, pinned, recent, m_query));
  Q_EMIT stateChanged();
}

bool LauncherAppletController::activate(const QString &entryId,
                                        const QString &actionId)
{
  if (!m_launchGranted) {
    publishFeedback(QStringLiteral("Application launching is not granted"));
    return false;
  }
  if (m_executor == nullptr) {
    publishFeedback(QStringLiteral("Launching is unavailable"));
    return false;
  }
  const LaunchOutcome outcome = m_executor->launch(entryId, actionId);
  if (!outcome.ok()) {
    publishFeedback(outcome.diagnostic);
    return false;
  }
  clearFeedback();
  if (m_persistence != nullptr)
    m_persistence->recordLaunch(entryId);
  return true;
}

bool LauncherAppletController::pin(const QString &entryId)
{
  if (m_persistence == nullptr)
    return false;
  return m_persistence->pin(entryId) == PersistenceMutation::Applied;
}

bool LauncherAppletController::unpin(const QString &entryId)
{
  if (m_persistence == nullptr)
    return false;
  return m_persistence->unpin(entryId) == PersistenceMutation::Applied;
}

bool LauncherAppletController::movePinnedUp(const QString &entryId)
{
  if (m_persistence == nullptr)
    return false;
  return m_persistence->movePinnedUp(entryId) == PersistenceMutation::Applied;
}

bool LauncherAppletController::movePinnedDown(const QString &entryId)
{
  if (m_persistence == nullptr)
    return false;
  return m_persistence->movePinnedDown(entryId) == PersistenceMutation::Applied;
}

bool LauncherAppletController::clearRecent()
{
  if (m_persistence == nullptr)
    return false;
  return m_persistence->clearRecent() == PersistenceMutation::Applied;
}

void LauncherAppletController::clearFeedback()
{
  publishFeedback({});
}

void LauncherAppletController::publishFeedback(const QString &message)
{
  if (m_feedback == message)
    return;
  m_feedback = message;
  Q_EMIT feedbackChanged();
}

} // namespace QindaQt::Shell::Launcher
