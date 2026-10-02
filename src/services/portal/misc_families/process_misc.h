// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "misc_ui.h"
#include <qindaqt/services/portal/session_binding.h>
#include <QProcess>
namespace QindaQt::Services::Portal {
// Same ownership/ordinary-FD contract as ProcessChooser; the print FD is
// duplicated before returning to the backend caller and closed after exec.
class ProcessMisc final : public MiscUi {
    Q_OBJECT
public:
    ProcessMisc(PortalSessionBinding &, AccessConsent &, QString executable);
    ~ProcessMisc() override;
    bool admitted() const override;
    void present(RequestToken, const QJsonObject &, int printFd = -1) override;
    void cancel(RequestToken) override;
private:
    void complete(int, QProcess::ExitStatus);
    void fail(RequestToken);
    PortalSessionBinding &m_binding; AccessConsent &m_authority;
    QString m_executable; QProcess m_process;
    RequestToken m_token = 0; QByteArray m_output;
    int m_fd = -1, m_printFd = -1;
};
}
