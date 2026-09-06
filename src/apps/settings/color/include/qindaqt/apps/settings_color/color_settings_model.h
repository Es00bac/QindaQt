// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_client/client.h>
#include <qindaqt/services/display_color_assignment/assignment_store.h>
#include <qindaqt/services/display_color_discovery/profile_discovery.h>
#include <qindaqt/services/display_writer/output_management_port.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>

#include <memory>
#include <optional>

namespace QindaQt::Apps::SettingsColor {

// Route-owned projection and closed intent facade for the Color Settings
// route. It composes only public boundaries: the borrowed same-thread public
// Display1 client (output inventory), the public C1 discovery/import seam
// (injected roots; catalog and user import), and the public C1 assignment
// store (Settings1 draft/apply persistence), and an optional owned public
// display-writer port used by production to apply profiles. The caller owns
// and outlives the four borrowed collaborators. QML receives copied values
// only and never sees a transport or compositor object.
//
// AGENT-CONTRACT: displayed availability and final dispatch share one
// admission predicate pinned to exact Display1 owner/epoch/revision truth
// plus confirmed usable Settings1 document authority. Success stays fenced
// until the store reports the typed outcome; conflict and uncertain outcomes
// are surfaced and never replayed. A persisted assignment is reported active
// only after the compositor confirms the matching writer request.
class ColorSettingsModel final
    : public QObject,
      private QindaQt::DisplayWriter::OutputManagementObserver {
  Q_OBJECT
  Q_PROPERTY(bool loading READ loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded READ degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable READ unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale READ stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
  Q_PROPERTY(bool retryAvailable READ retryAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool compositorApplicationSupported READ
                 compositorApplicationSupported NOTIFY viewChanged)
  Q_PROPERTY(bool catalogScanned READ catalogScanned NOTIFY viewChanged)
  Q_PROPERTY(bool catalogComplete READ catalogComplete NOTIFY viewChanged)
  Q_PROPERTY(bool unassignAvailable READ unassignAvailable NOTIFY viewChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
  Q_PROPERTY(
      QString operationStatusText READ operationStatusText NOTIFY viewChanged)
  Q_PROPERTY(QString importStatusText READ importStatusText NOTIFY viewChanged)
  Q_PROPERTY(
      QString catalogSummaryText READ catalogSummaryText NOTIFY viewChanged)
  Q_PROPERTY(QString displayEpoch READ displayEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong displayRevision READ displayRevision NOTIFY viewChanged)
  Q_PROPERTY(
      qulonglong settingsRevision READ settingsRevision NOTIFY viewChanged)
  Q_PROPERTY(QString selectedOutputId READ selectedOutputId NOTIFY viewChanged)
  Q_PROPERTY(
      QString selectedOutputName READ selectedOutputName NOTIFY viewChanged)
  Q_PROPERTY(QVariantList outputRows READ outputRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList profileRows READ profileRows NOTIFY viewChanged)
  Q_PROPERTY(QVariantList inactiveAssignmentRows READ inactiveAssignmentRows
                 NOTIFY viewChanged)

public:
  explicit ColorSettingsModel(
      QindaQt::DisplayClient::Client &displayClient,
      QindaQt::Services::SettingsClient::SettingsClient &settingsClient,
      QindaQt::DisplayColor::SettingsAssignmentStore &store,
      QindaQt::DisplayColor::ProfileDiscovery &discovery,
      QObject *parent = nullptr);
  ~ColorSettingsModel() override;

  // Installs the production compositor application boundary before route
  // activation. Tests may omit it to exercise persistence in isolation.
  void installColorApplicationPort(
      std::unique_ptr<QindaQt::DisplayWriter::OutputManagementPort> port);

  [[nodiscard]] bool loading() const noexcept;
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool degraded() const noexcept;
  [[nodiscard]] bool unavailable() const noexcept;
  [[nodiscard]] bool stale() const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool retryAvailable() const noexcept;
  [[nodiscard]] bool compositorApplicationSupported() const noexcept;
  [[nodiscard]] bool catalogScanned() const noexcept {
    return m_catalogScanned;
  }
  [[nodiscard]] bool catalogComplete() const noexcept;
  [[nodiscard]] bool unassignAvailable() const;
  [[nodiscard]] QString statusText() const;
  [[nodiscard]] const QString &errorText() const noexcept {
    return m_errorText;
  }
  [[nodiscard]] const QString &operationStatusText() const noexcept {
    return m_operationStatusText;
  }
  [[nodiscard]] const QString &importStatusText() const noexcept {
    return m_importStatusText;
  }
  [[nodiscard]] QString catalogSummaryText() const;
  [[nodiscard]] QString displayEpoch() const;
  [[nodiscard]] qulonglong displayRevision() const;
  [[nodiscard]] qulonglong settingsRevision() const;
  [[nodiscard]] const QString &selectedOutputId() const noexcept {
    return m_selectedOutputId;
  }
  [[nodiscard]] QString selectedOutputName() const;
  [[nodiscard]] QVariantList outputRows() const;
  [[nodiscard]] QVariantList profileRows() const;
  [[nodiscard]] QVariantList inactiveAssignmentRows() const;

  Q_INVOKABLE bool retry();
  Q_INVOKABLE void setRouteActive(bool active);
  Q_INVOKABLE bool selectOutput(const QString &stableId);
  Q_INVOKABLE bool assignProfile(const QString &profileId);
  Q_INVOKABLE bool unassignSelected();
  Q_INVOKABLE bool importProfile(const QUrl &source);

Q_SIGNALS:
  void viewChanged();
  void actionRejected(const QString &reason);

private:
  [[nodiscard]] bool hasDisplaySnapshot() const noexcept;
  [[nodiscard]] bool usableDisplayLineage() const noexcept;
  [[nodiscard]] QString assignmentAdmissionBase() const;
  [[nodiscard]] QString assignAdmission(const QString &profileId) const;
  [[nodiscard]] QString unassignAdmission() const;
  [[nodiscard]] const QindaQt::DisplayColor::IccProfileDescriptor *
  findProfile(const QString &profileId) const;
  [[nodiscard]] const QindaQt::DisplayColor::DiscoveredProfile *
  findDiscoveredProfile(const QString &profileId) const;
  [[nodiscard]] QString assignedProfileFor(const QString &stableId) const;
  [[nodiscard]] bool displayChangeInProgress() const noexcept;
  void refreshCatalog();
  [[nodiscard]] bool
  submitDraft(const QindaQt::DisplayColor::ColorAssignmentDraft &draft,
      const QString &progressText);
  void handleApplyFinished(
      const QindaQt::DisplayColor::AssignmentApplyOutcome &outcome);
  void synchronizeAuthority();
  void applyPendingColorProfile();
  void reconcileSavedProfiles();
  void outputManagementOwnerChanged(quint64 ownerGeneration,
                                    bool available) override;
  void outputManagementCompleted(
      quint64 ownerGeneration, quint64 requestId,
      QindaQt::DisplayWriter::CompletionOutcome outcome) override;
  void reject(const QString &reason);
  [[nodiscard]] QString failureText(const QString &reason) const;

  QindaQt::DisplayClient::Client &m_displayClient;
  QindaQt::Services::SettingsClient::SettingsClient &m_settingsClient;
  QindaQt::DisplayColor::SettingsAssignmentStore &m_store;
  QindaQt::DisplayColor::ProfileDiscovery &m_discovery;
  QindaQt::DisplayColor::DiscoveryResult m_catalog;
  QHash<QString, QByteArray> m_importLineageByProfileId;
  QString m_selectedOutputId;
  bool m_catalogScanned = false;
  bool m_routeActive = false;
  bool m_retrying = false;
  QString m_errorText;
  QString m_operationStatusText;
  QString m_importStatusText;
  struct PendingColorApplication {
    QString connectorName;
    QString profilePath;
  };
  std::unique_ptr<QindaQt::DisplayWriter::OutputManagementPort> m_colorPort;
  std::optional<PendingColorApplication> m_pendingColorApplication;
  QList<PendingColorApplication> m_reconciliationQueue;
  quint64 m_colorOwnerGeneration = 0;
  quint64 m_colorRequestId = 0;
  bool m_colorPortAvailable = false;
  bool m_colorApplyInFlight = false;
  bool m_currentApplyIsReconciliation = false;
  bool m_reconciliationNeeded = true;
};

} // namespace QindaQt::Apps::SettingsColor
