// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/chooser_ui.h>
#include <qindaqt/services/portal/session_binding.h>
#include <QProcess>
namespace QindaQt::Services::Portal {
// Owns one ordinary-display child, no GUI in the resident. Binding and live
// readonly authority outlive this same-thread port. Executable is a trusted
// composition input; display FD is freshly admitted, with no ambient fallback.
// Updates use bounded newline JSON on private stdin, never a bus/UI test seam.
class ProcessChooser final : public ChooserUi {
    Q_OBJECT
public:
    ProcessChooser(PortalSessionBinding &, AccessConsent &authority, QString executable);
    ~ProcessChooser() override;
    bool admitted() const override;
    void openFile(RequestToken, const FileChooserRequest &) override;
    void chooseApplication(RequestToken, const AppChooserRequest &) override;
    void updateApplications(RequestToken, const ApplicationCandidates &) override;
    void cancel(RequestToken) override;
private:
    void start(RequestToken, const QJsonObject &);
    void complete(int, QProcess::ExitStatus);
    void fail(RequestToken);
    PortalSessionBinding &m_binding; AccessConsent &m_authority;
    QString m_executable; QProcess m_process;
    RequestToken m_token = 0; QByteArray m_output;
    int m_fd = -1;
};
} // namespace QindaQt::Services::Portal
