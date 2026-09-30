// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/secret_portal/secret_broker.h>
#include <QObject>
#include <functional>
#include <memory>
namespace QindaQt::Services::SecretPortal::Private {
// Owns a duplicate and secure pages until complete/cancel. Only FIFO or stream
// socket sinks are admitted; FIFO open-file-description becomes nonblocking.
// Every attempted write rechecks readonly admission; success means all32 bytes
// were accepted by the FD, not that its remote consumer persisted them.
class FdWriter final:public QObject {
public:
    FdWriter(int borrowedFd,std::function<bool()> admitted,std::function<void(bool)> completed,QObject *parent=nullptr);
    ~FdWriter() override;
    bool valid() const;
    void start(SecretPages);
    void cancel();
private:
    class Data;std::unique_ptr<Data> d;
};
}
