// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace QindaQt::Tests::DisplayPageSupport {

class StubDisplayModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY stateChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY stateChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY stateChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY stateChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY stateChanged)
  Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY stateChanged)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY stateChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY stateChanged)

  Q_PROPERTY(QVariantList outputs MEMBER outputs NOTIFY outputsChanged)
  Q_PROPERTY(QString selectedOutputId MEMBER selectedOutputId WRITE
                 setSelectedOutputId NOTIFY selectedOutputIdChanged)
  Q_PROPERTY(QVariantMap selectedOutput MEMBER selectedOutput NOTIFY
                 selectedOutputChanged)

  Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY draftChanged)
  Q_PROPERTY(bool draftValid MEMBER draftValid NOTIFY draftChanged)
  Q_PROPERTY(
      QString draftErrorMessage MEMBER draftErrorMessage NOTIFY draftChanged)
  Q_PROPERTY(QVariantMap fieldErrors MEMBER fieldErrors NOTIFY draftChanged)
  Q_PROPERTY(QVariantList warnings MEMBER warnings NOTIFY draftChanged)
  Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY stateChanged)

  Q_PROPERTY(bool inTransaction MEMBER inTransaction NOTIFY transactionChanged)
  Q_PROPERTY(bool awaitingConfirmation MEMBER awaitingConfirmation NOTIFY
                 transactionChanged)
  Q_PROPERTY(int transactionRemainingSeconds MEMBER transactionRemainingSeconds
                 NOTIFY transactionCountdownChanged)
  Q_PROPERTY(QString transactionStatusText MEMBER transactionStatusText NOTIFY
                 transactionChanged)
  Q_PROPERTY(QString activeTransactionId MEMBER activeTransactionId NOTIFY
                 transactionChanged)

public:
  bool loading = false;
  bool ready = true;
  bool busy = false;
  bool unavailable = false;
  bool degraded = false;
  bool canEdit = true;
  QString statusText;
  QString errorText;

  QVariantList outputs;
  QString selectedOutputId = QStringLiteral("edid:dp1");
  QVariantMap selectedOutput;

  bool draftDirty = false;
  bool draftValid = true;
  QString draftErrorMessage;
  QVariantMap fieldErrors;
  QVariantList warnings;
  bool applyAvailable = false;

  bool inTransaction = false;
  bool awaitingConfirmation = false;
  int transactionRemainingSeconds = 15;
  QString transactionStatusText;
  QString activeTransactionId;

  int appliedCount = 0;
  int canceledCount = 0;
  int confirmedCount = 0;
  int revertedCount = 0;
  int retriedCount = 0;
  int positionSetCount = 0;
  QString lastPositionStableId;

  explicit StubDisplayModel(QObject *parent = nullptr) : QObject(parent) {
    setupDefaultOutputs();
  }

  void setupDefaultOutputs() {
    QVariantMap mode1{
        {QStringLiteral("id"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("label"), QStringLiteral("3840 × 2160 @ 60 Hz")},
        {QStringLiteral("preferred"), true},
    };
    QVariantMap mode2{
        {QStringLiteral("id"), QStringLiteral("1920x1080@60")},
        {QStringLiteral("label"), QStringLiteral("1920 × 1080 @ 60 Hz")},
        {QStringLiteral("preferred"), false},
    };
    QVariantList modes{mode1, mode2};

    selectedOutput = {
        {QStringLiteral("stableId"), QStringLiteral("edid:dp1")},
        {QStringLiteral("connectorName"), QStringLiteral("DP-1")},
        {QStringLiteral("label"), QStringLiteral("Main Monitor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("primary"), true},
        {QStringLiteral("modeId"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("positionX"), 0},
        {QStringLiteral("positionY"), 0},
        {QStringLiteral("logicalWidth"), 1920},
        {QStringLiteral("logicalHeight"), 1080},
        {QStringLiteral("scale"), 2.0},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("modes"), modes},
    };

    outputs = {selectedOutput};
    outputsMap[QStringLiteral("edid:dp1")] = selectedOutput;
    baselineOutputsMap = outputsMap;
  }

  QMap<QString, QVariantMap> outputsMap;
  QMap<QString, QVariantMap> baselineOutputsMap;

  void setupTwoOutputs() {
    QVariantMap mode1{
        {QStringLiteral("id"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("label"), QStringLiteral("3840 × 2160 @ 60 Hz")},
        {QStringLiteral("preferred"), true},
    };
    QVariantList modes{mode1};

    QVariantMap out1{
        {QStringLiteral("stableId"), QStringLiteral("edid:dp1")},
        {QStringLiteral("connectorName"), QStringLiteral("DP-1")},
        {QStringLiteral("label"), QStringLiteral("Main Monitor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("primary"), true},
        {QStringLiteral("modeId"), QStringLiteral("3840x2160@60")},
        {QStringLiteral("positionX"), 0},
        {QStringLiteral("positionY"), 0},
        {QStringLiteral("logicalWidth"), 1920},
        {QStringLiteral("logicalHeight"), 1080},
        {QStringLiteral("scale"), 2.0},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("modes"), modes},
    };

    QVariantMap out2{
        {QStringLiteral("stableId"), QStringLiteral("edid:hdmi1")},
        {QStringLiteral("connectorName"), QStringLiteral("HDMI-1")},
        {QStringLiteral("label"), QStringLiteral("Side Monitor")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("primary"), false},
        {QStringLiteral("modeId"), QStringLiteral("1920x1080@60")},
        {QStringLiteral("positionX"), 1920},
        {QStringLiteral("positionY"), 0},
        {QStringLiteral("logicalWidth"), 1920},
        {QStringLiteral("logicalHeight"), 1080},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("transform"), QStringLiteral("normal")},
        {QStringLiteral("modes"), modes},
    };

    outputsMap.clear();
    outputsMap[QStringLiteral("edid:dp1")] = out1;
    outputsMap[QStringLiteral("edid:hdmi1")] = out2;
    baselineOutputsMap = outputsMap;

    outputs = {out1, out2};
    selectedOutputId = QStringLiteral("edid:dp1");
    selectedOutput = out1;
  }

  Q_INVOKABLE void setSelectedOutputId(const QString &id) {
    selectedOutputId = id;
    if (outputsMap.contains(id)) {
      selectedOutput = outputsMap.value(id);
    }
    Q_EMIT selectedOutputIdChanged(id);
    Q_EMIT selectedOutputChanged();
  }

  Q_INVOKABLE bool setOutputEnabled(const QString &stableId, bool enabled) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("enabled")] = enabled;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputPrimary(const QString &stableId) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("primary")] = true;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputMode(const QString &stableId,
                                 const QString &modeId) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("modeId")] = modeId;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputScale(const QString &stableId, double scale) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("scale")] = scale;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputTransform(const QString &stableId,
                                      const QString &t) {
    Q_UNUSED(stableId);
    selectedOutput[QStringLiteral("transform")] = t;
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool setOutputPosition(const QString &stableId, int x, int y) {
    ++positionSetCount;
    lastPositionStableId = stableId;
    if (outputsMap.contains(stableId)) {
      auto map = outputsMap.value(stableId);
      map[QStringLiteral("positionX")] = x;
      map[QStringLiteral("positionY")] = y;
      outputsMap[stableId] = map;
      if (selectedOutputId == stableId) {
        selectedOutput = map;
      }
    } else {
      selectedOutput[QStringLiteral("positionX")] = x;
      selectedOutput[QStringLiteral("positionY")] = y;
    }
    draftDirty = true;
    applyAvailable = true;
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  void publishExternalOutputPosition(const QString &stableId, int x, int y) {
    auto map = outputsMap.value(stableId);
    map[QStringLiteral("positionX")] = x;
    map[QStringLiteral("positionY")] = y;
    outputsMap[stableId] = map;
    if (selectedOutputId == stableId) {
      selectedOutput = map;
    }
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
  }

  Q_INVOKABLE bool applyDraft() {
    ++appliedCount;
    return true;
  }

  Q_INVOKABLE bool cancelDraft() {
    ++canceledCount;
    draftDirty = false;
    applyAvailable = false;
    outputsMap = baselineOutputsMap;
    if (outputsMap.contains(selectedOutputId)) {
      selectedOutput = outputsMap.value(selectedOutputId);
    }
    Q_EMIT selectedOutputChanged();
    Q_EMIT draftChanged();
    Q_EMIT stateChanged();
    return true;
  }

  Q_INVOKABLE bool confirmTransaction() {
    ++confirmedCount;
    return true;
  }

  Q_INVOKABLE bool revertTransaction() {
    ++revertedCount;
    return true;
  }

  Q_INVOKABLE void retry() { ++retriedCount; }

Q_SIGNALS:
  void stateChanged();
  void outputsChanged();
  void selectedOutputIdChanged(const QString &selectedOutputId);
  void selectedOutputChanged();
  void draftChanged();
  void transactionChanged();
  void transactionCountdownChanged();
};

} // namespace QindaQt::Tests::DisplayPageSupport
