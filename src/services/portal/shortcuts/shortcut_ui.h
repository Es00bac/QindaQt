// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <qindaqt/services/portal/session_binding.h>
#include <QJsonObject>
#include <QProcess>
namespace QindaQt::Services::Portal {
// Independent shortcut editing UI port. Production child inherits only the
// freshly admitted display FD; binding/lock authority outlive this object.
class ShortcutUi : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool admitted() const = 0;
    virtual void ask(RequestToken, const QJsonObject &) = 0;
    virtual void cancel(RequestToken) = 0;
Q_SIGNALS:
    void completed(RequestToken, RequestResponse, const QJsonObject &);
    void authorityLost();
};
class ProcessShortcuts final : public ShortcutUi {
    Q_OBJECT
public:
    ProcessShortcuts(PortalSessionBinding &, AccessConsent &, QString executable);
    ~ProcessShortcuts() override;
    bool admitted() const override;
    void ask(RequestToken, const QJsonObject &) override;
    void cancel(RequestToken) override;
private:
    void start(RequestToken, const QJsonObject &);
    void complete(int, QProcess::ExitStatus);
    void fail(RequestToken);
    PortalSessionBinding &m_binding; AccessConsent &m_authority;
    QString m_executable; QProcess m_process;
    RequestToken m_token = 0; QByteArray m_output; int m_fd = -1;
};
}
