// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/services/portal/chooser_types.h>
namespace QindaQt::Services::Portal {
// Same-thread borrowed asynchronous presentation boundary, no actor policy.
// cancel retires synchronously; late completions never authorize a dead token.
// App updates replace all offered candidates, retaining selection only if
// still offered. This object and dependencies outlive its family adaptors.
class ChooserUi : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual bool admitted() const = 0;
    virtual void openFile(RequestToken, const FileChooserRequest &) = 0;
    virtual void chooseApplication(RequestToken, const AppChooserRequest &) = 0;
    virtual void updateApplications(RequestToken, const ApplicationCandidates &) = 0;
    virtual void cancel(RequestToken) = 0;
Q_SIGNALS:
    void completed(RequestToken, RequestResponse, const QJsonObject &results);
    void authorityLost();
};
} // namespace QindaQt::Services::Portal
