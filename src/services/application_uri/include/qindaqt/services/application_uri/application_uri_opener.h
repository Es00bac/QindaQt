// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <qindaqt/apps/settings_default_apps/default_applications_store.h>
#include <QObject>
#include <QUrl>
#include <functional>
#include <memory>
namespace QindaQt::Services::ApplicationUri {
enum class UriOpenResult { Started, NoHandler, UnsupportedHandler, Unavailable, Failed, Cancelled, Busy };
// Public native URI handoff, never QDesktopServices or a recursive portal call.
// Same-thread async port. A Started result means native process exec succeeded,
// not that a message was sent. cancel retires pending work; once handed to the
// application, the independent draft window/process is owned by that app.
class ApplicationUriOpener : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void open(quint64 token, const QUrl &, const QString &activationToken) = 0;
    virtual void cancel(quint64 token) = 0;
Q_SIGNALS:
    void completed(quint64 token, UriOpenResult result);
};
// Borrows a store plus readonly/non-reentrant per-request admission and owned-FD
// display opener. All captures/store outlive this object, on one Qt thread.
// Copies the public installed application scan. Caller refreshes that scan;
// every handoff resolves current MIME policy and uses only catalog-retained
// validated Exec bytes. No caller-supplied program, shell or ambient display.
// The trusted relay executable is a composition input, never a portal option.
class DefaultApplicationUriOpener final : public ApplicationUriOpener {
    Q_OBJECT
public:
    DefaultApplicationUriOpener(QindaQt::Apps::SettingsDefaultApps::DefaultApplicationsStore &,
        QindaQt::ApplicationCatalog::DirectoryScan, QString relayExecutable,
        std::function<bool(quint64)> admission, std::function<int()> openDisplay,
        QObject *parent = nullptr);
    ~DefaultApplicationUriOpener() override;
    void setApplications(QindaQt::ApplicationCatalog::DirectoryScan);
    void open(quint64, const QUrl &, const QString &) override;
    void cancel(quint64) override;
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Services::ApplicationUri
