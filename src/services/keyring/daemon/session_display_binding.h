// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QTimer>
namespace qindaqt::keyring::service {
// GUI-thread platform collaborator. Binding pins one session/compositor lineage.
// openPromptConnection returns an owned connected fd, -1 rejected, or -2 for
// untouched legacy mode. Any metadata attempt permanently fences
// fallback; revocation cancels prompts, never chooses another display.
class SessionDisplayBinding final : public QObject {
    Q_OBJECT
public:
    SessionDisplayBinding(QDBusConnection,QString runtime,bool required,QObject *parent=nullptr);
    ~SessionDisplayBinding() override;
    bool attach(const QString &sessionOwner,const QString &basename);
    int openPromptConnection();
    bool live();
    QString basename() const { return basename_; }
    QString compositorOwner() const { return compositorOwner_; }
    quint64 compositorPid() const { return static_cast<quint64>(compositorPid_); }
Q_SIGNALS:
    void revoked();
    void attached(); // Emitted only after live owner/PID/ordinary-peer admission.
private:
    int connectPeer(const QString &,qint64 expected,int *pidfd=nullptr);
    void clear();
    bool sameOwners();
    QDBusConnection bus_;
    QString runtime_,sessionOwner_,compositorOwner_,basename_;
    qint64 compositorPid_=0;
    int probe_=-1,pidfd_=-1;
    bool required_=false;
    QTimer check_;
    QDBusServiceWatcher watcher_;
};
}
