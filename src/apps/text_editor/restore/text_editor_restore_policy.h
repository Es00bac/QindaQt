// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
struct CommitOutcome;
} // namespace QindaQt::Services::SettingsClient

namespace QindaQt::Apps::TextEditor {

namespace TextEditorKeys {
inline constexpr QLatin1String RestoreDocuments{
    QLatin1String("services.textEditorRestoreDocuments")};
[[nodiscard]] QStringList scopedKeys();
} // namespace TextEditorKeys

enum class RestorePolicyResult { Applied, Conflict, Failed, Uncertain };

// AGENT-CONTRACT: Confirmed policy comes only from one complete Settings1
// snapshot. Loss or malformed truth immediately falls back to false; conflicts
// require explicit re-apply; timeout/owner/bus uncertainty is never replayed.
// This component persists policy only and never reads or writes the path state.
class TextEditorRestorePolicy final : public QObject {
  Q_OBJECT

public:
  explicit TextEditorRestorePolicy(
      QindaQt::Services::SettingsClient::SettingsClient &client,
      QObject *parent = nullptr);

  [[nodiscard]] bool enabled() const { return m_enabled; }
  [[nodiscard]] bool baselineReceived() const { return m_baselineReceived; }
  [[nodiscard]] bool writePending() const { return m_writePending; }
  [[nodiscard]] bool requestEnabled(bool enabled);

signals:
  void policyChanged();
  void applyFinished(QindaQt::Apps::TextEditor::RestorePolicyResult result,
                     const QString &message);

private:
  void handleSnapshot();
  void handleClientState();
  void
  handleCommit(const QindaQt::Services::SettingsClient::CommitOutcome &outcome);
  void handleUncertain(const QString &message);
  void resetToDefault();
  void finish(RestorePolicyResult result, const QString &message);

  QindaQt::Services::SettingsClient::SettingsClient &m_client;
  bool m_enabled = false;
  bool m_baselineReceived = false;
  bool m_writePending = false;
  bool m_requested = false;
};

} // namespace QindaQt::Apps::TextEditor
