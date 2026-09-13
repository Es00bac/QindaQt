// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_client/client.h>

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>

#include <optional>

namespace QindaQt::Apps::SettingsPower {

// Route-owned external-display brightness rows and intent facade over one
// borrowed, same-thread public Display client (ADR-0150). The caller owns and
// outlives the model. QML receives copied values and route-local row IDs.
//
// AGENT-CONTRACT: displayed availability and final dispatch share one
// admission predicate pinned to the exact client owner, service epoch, and
// published brightness revision. One debounce or operation may be live at a
// time; success stays fenced until joined brightness at or beyond the result's
// observed revision arrives. Busy, refused, uncertain, and owner-interrupted
// work is never replayed. Internal panels stay with Power1 (ADR-0148).
class ExternalDisplayBrightnessModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows READ rows NOTIFY viewChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText READ operationStatusText NOTIFY viewChanged)

public:
  explicit ExternalDisplayBrightnessModel(DisplayClient::Client &client,
                                          QObject *parent = nullptr);

  [[nodiscard]] QVariantList rows() const;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }
  [[nodiscard]] const QString &operationStatusText() const noexcept {
    return m_operationStatusText;
  }

  // Normalized 0-10000, the same scale as Display1 and the Power rows.
  Q_INVOKABLE bool requestBrightness(const QString &rowId, int normalized);

Q_SIGNALS:
  void viewChanged();
  void actionRejected(const QString &reason);

private:
  struct Lineage {
    QString owner;
    QString epoch;
    quint64 revision = 0;
  };
  struct Target {
    QString stableId;
    bool settable = false;
    quint32 value = 0;
  };
  struct Debounced {
    QString rowId;
    QString stableId;
    quint32 value = 0;
    Lineage lineage;
  };
  struct Pending {
    quint64 requestId = 0;
    QString stableId;
    Lineage lineage;
    quint32 expected = 0;
  };
  struct Convergence {
    QString stableId;
    QString owner;
    QString epoch;
    quint64 minimumRevision = 0;
    quint32 expected = 0;
  };

  [[nodiscard]] std::optional<Lineage> currentLineage() const;
  [[nodiscard]] std::optional<Target> findTarget(const QString &rowId) const;
  [[nodiscard]] QString admission(const QString &rowId, bool replacingDebounce) const;
  void dispatchDebounced();
  void handleOperationCompleted(quint64 requestId,
                                const Display::OperationResult &result);
  void settleConvergence();
  void synchronizeAuthority();
  void reject(const QString &reason);
  [[nodiscard]] QString failureText(const QString &reason) const;

  DisplayClient::Client &m_client;
  QTimer m_debounceTimer;
  QTimer m_convergenceTimer;
  std::optional<Debounced> m_debounce;
  std::optional<Pending> m_pending;
  std::optional<Convergence> m_convergence;
  QString m_errorText;
  QString m_operationStatusText;
};

} // namespace QindaQt::Apps::SettingsPower
