// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "tests/apps/settings/audio/stub_audio_settings_model.h"
#include "tests/apps/settings/network/stub_network_settings_model.h"
#include "tests/apps/settings/bluetooth/stub_bluetooth_settings_model.h"
#include "tests/apps/settings/power/stub_power_settings_model.h"
#include "tests/apps/settings/clipboard/stub_clipboard_settings_model.h"
#include "tests/apps/settings/customize/stub_customize_settings_model.h"

#include <QObject>
#include <QVariant>

namespace QindaQt::Apps::SettingsCenter::TestSupport {

using QindaQt::Apps::SettingsAudio::TestSupport::StubAudioSettingsModel;
using QindaQt::Apps::SettingsNetwork::TestSupport::StubNetworkSettingsModel;
using QindaQt::Apps::SettingsBluetooth::TestSupport::StubBluetoothSettingsModel;
using QindaQt::Apps::SettingsPower::TestSupport::StubPowerSettingsModel;
using QindaQt::Apps::SettingsClipboard::TestSupport::StubClipboardSettingsModel;
using QindaQt::Apps::SettingsCustomize::TestSupport::StubCustomizeSettingsModel;

class StubQuietingModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool enabled MEMBER enabled NOTIFY changed)
  Q_PROPERTY(bool canToggle MEMBER canToggle NOTIFY changed)
  Q_PROPERTY(bool conflict MEMBER conflict NOTIFY changed)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY changed)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY changed)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY changed)
  Q_PROPERTY(int requestCount MEMBER requestCount NOTIFY changed)
  Q_PROPERTY(int retryCount MEMBER retryCount NOTIFY changed)
  Q_PROPERTY(int applyCount MEMBER applyCount NOTIFY changed)

public:
  bool enabled = false;
  bool canToggle = true;
  bool conflict = false;
  bool unavailable = false;
  QString statusText;
  QString errorText;
  int requestCount = 0;
  int retryCount = 0;
  int applyCount = 0;

  explicit StubQuietingModel(QObject *parent = nullptr) : QObject(parent) {}

  Q_INVOKABLE bool requestSet(bool val) {
    ++requestCount;
    enabled = val;
    Q_EMIT changed();
    return true;
  }
  Q_INVOKABLE void retry() {
    ++retryCount;
    Q_EMIT changed();
  }
  Q_INVOKABLE bool applyMyChoice() {
    ++applyCount;
    Q_EMIT changed();
    return true;
  }

Q_SIGNALS:
  void changed();
};

class StubAppearanceModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY stateChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY stateChanged)
  Q_PROPERTY(bool saving MEMBER saving NOTIFY stateChanged)
  Q_PROPERTY(bool conflict MEMBER conflict NOTIFY stateChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY stateChanged)
  Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY stateChanged)
  Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY draftChanged)
  Q_PROPERTY(bool draftValid MEMBER draftValid NOTIFY draftChanged)
  Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY stateChanged)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY stateChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY stateChanged)
  Q_PROPERTY(QString saveResultsText MEMBER saveResultsText NOTIFY stateChanged)
  Q_PROPERTY(bool saveResultsHaveFailure MEMBER saveResultsHaveFailure NOTIFY
                 stateChanged)
  Q_PROPERTY(QVariantMap draft MEMBER draft NOTIFY draftChanged)
  Q_PROPERTY(QVariantMap fieldErrors MEMBER fieldErrors NOTIFY draftChanged)
  Q_PROPERTY(QVariantList installedThemes MEMBER installedThemes CONSTANT)
  Q_PROPERTY(QString resolvedThemeId MEMBER resolvedThemeId NOTIFY draftChanged)
  Q_PROPERTY(bool configuredThemeInstalled MEMBER configuredThemeInstalled
                 NOTIFY draftChanged)
  Q_PROPERTY(QString fallbackNotice MEMBER fallbackNotice NOTIFY draftChanged)

public:
  bool loading = false;
  bool ready = true;
  bool saving = false;
  bool conflict = false;
  bool unavailable = false;
  bool canEdit = true;
  bool draftDirty = false;
  bool draftValid = true;
  bool applyAvailable = false;
  QString statusText;
  QString errorText;
  QString saveResultsText;
  bool saveResultsHaveFailure = false;
  QVariantMap draft{
      {QStringLiteral("appearance.theme"), QStringLiteral("qinda-dark")},
      {QStringLiteral("appearance.colorScheme"), QStringLiteral("dark")},
      {QStringLiteral("appearance.wallpaper"), QString{}},
      {QStringLiteral("appearance.wallpaperMode"), QStringLiteral("scaled")},
      {QStringLiteral("display.uiScale"), 1.0},
      {QStringLiteral("fonts.family"), QStringLiteral("Noto Sans")},
      {QStringLiteral("fonts.pointSize"), 10.0},
      {QStringLiteral("fonts.antialiasing"), true},
      {QStringLiteral("fonts.hinting"), QStringLiteral("slight")},
      {QStringLiteral("fonts.subpixelOrder"), QStringLiteral("rgb")},
  };
  QVariantMap fieldErrors;
  QVariantList installedThemes;
  QString resolvedThemeId;
  bool configuredThemeInstalled = true;
  QString fallbackNotice;

  explicit StubAppearanceModel(QObject *parent = nullptr) : QObject(parent) {}

  Q_INVOKABLE bool setDraftValue(const QString &key, const QVariant &value) {
    draft[key] = value;
    draftDirty = true;
    Q_EMIT draftChanged();
    return true;
  }
  Q_INVOKABLE bool applyDraft() { return true; }
  Q_INVOKABLE bool cancelDraft() {
    draftDirty = false;
    Q_EMIT draftChanged();
    return true;
  }
  Q_INVOKABLE void retry() { Q_EMIT stateChanged(); }

Q_SIGNALS:
  void stateChanged();
  void draftChanged();
};

} // namespace QindaQt::Apps::SettingsCenter::TestSupport
