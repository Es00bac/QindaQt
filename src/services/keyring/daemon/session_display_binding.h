// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <QObject>
#include <QDBusConnection>
namespace qindaqt::keyring::service {
// Same-thread keyring session selection and legacy compatibility policy. The
// public platform collaborator owns all bus/socket/PIDFD identity proof.
// openPromptConnection transfers an owned ordinary FD, -1 rejected, or -2 only
// for untouched legacy activation. Any metadata attempt permanently retires
// fallback; revocation cancels prompts and never chooses another display.
class SessionDisplayBinding final : public QObject {
    Q_OBJECT
public:
    SessionDisplayBinding(QDBusConnection,QString runtime,bool required,QObject *parent=nullptr);
    ~SessionDisplayBinding() override;
    bool attach(const QString &sessionOwner,const QString &basename);
    int openPromptConnection();
    bool live();
    QString basename() const;
    QString compositorOwner() const;
    quint64 compositorPid() const;
Q_SIGNALS:
    void revoked();
    void attached();
private:
    QString selectedSessionOwner_;
    bool required_=false;
    QindaQt::Platform::Compositor::CompositorAttachment attachment_;
};
}
