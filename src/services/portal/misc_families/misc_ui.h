// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/access_consent.h>
#include <QJsonObject>
namespace QindaQt::Services::Portal {
// Borrowed Qt-thread presentation port. Implementations own the ordinary helper
// and any duplicated print descriptor. Retirement cancels synchronously; late
// output never authorizes a retired Request. No policy or ambient display here.
class MiscUi : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool admitted() const = 0;
    virtual void present(RequestToken, const QJsonObject &, int printFd = -1) = 0;
    virtual void cancel(RequestToken) = 0;
Q_SIGNALS:
    void completed(RequestToken, RequestResponse, const QJsonObject &);
    void authorityLost();
};
}
