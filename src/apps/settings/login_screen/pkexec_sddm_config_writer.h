// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_login_screen/login_screen_settings_model.h>

#include <QtCore/QProcess>

namespace QindaQt::Apps::SettingsLoginScreen {

// Production SddmConfigWriteClient: pipes the serialized change set to
// `pkexec <helper>` and reports the process outcome. All policy lives in
// the helper and its .policy file; this class is only the transport. The
// write is asynchronous because the authentication prompt may be answered
// minutes from now, and a frozen Settings window would look like a crash.
class PkexecSddmConfigWriter final : public SddmConfigWriteClient {
  Q_OBJECT
public:
  // `pkexecPath` and `helperPath` are injected so the composition root owns
  // path truth and tests can point at nothing at all.
  PkexecSddmConfigWriter(QString pkexecPath, QString helperPath,
                         QObject *parent = nullptr);
  ~PkexecSddmConfigWriter() override;

  void write(const SddmOwnedChangeSet &changes) override;
  [[nodiscard]] bool writeInFlight() const override {
    return m_process != nullptr;
  }

private:
  void onFinished(int exitCode, QProcess::ExitStatus status);

  QString m_pkexecPath;
  QString m_helperPath;
  QProcess *m_process = nullptr;
};

} // namespace QindaQt::Apps::SettingsLoginScreen
