// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "collection_repository.h"
#include <QSocketNotifier>
#include <QObject>
#include <map>
namespace qindaqt::keyring::service {
// Native/PAM seam, one bounded frame per connection. Peers must be euid,
// passwords reside in secure receive pages, never argv/env/files. No creation.
// QKR1/op/u8-ID-length/u16-old-length/u16-new-length, then ID/old/new bytes;
// lengths network byte order, ops 1 unlock, 2 authenticated rekey, 3 lock.
// Response QKR1 + u8 (0 acknowledged, 1 rejected). Rekey authenticates old
// password first. No response succeeds on uncertain persistence (ADR-0296).
class ControlServer final : public QObject {
    Q_OBJECT
public:
    ControlServer(QString runtimeDirectory, CollectionRepository &, int activatedFd = -1,
                  QObject *parent = nullptr);
    ~ControlServer() override;
Q_SIGNALS:
    void collectionStateChanged(const QString &id);
private:
    struct Client {
        int fd = -1;
        std::unique_ptr<QSocketNotifier> notifier;
        SecureBuffer frame{8262};
        std::size_t used = 0;
        ~Client();
    };
    void accept();
    void receive(quint64);
    void finish(quint64,bool);
    QString path_;
    PrivateDirectory runtime_;
    CollectionRepository &repository_;
    int listener_ = -1;
    bool ownsPath_ = false;
    std::unique_ptr<QSocketNotifier> notifier_;
    quint64 generation_ = 0;
    std::map<quint64,std::unique_ptr<Client>> clients_;
};
}
