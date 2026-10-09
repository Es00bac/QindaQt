// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/android_manager/runtime_port.h"
#include "qindaqt/android_manager/setup_port.h"
#include "qindaqt/shell_launcher/application_catalog.h"

#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <optional>

namespace QindaQt::AndroidManager {

// GUI-thread presentation/intent coordinator. Borrows its port (no transport
// or package/process reach); port destruction fails unavailable. No private
// data is persisted. One pending request is serialized and never replayed.
class ApplicationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList applications READ applications NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QString runtimeState READ runtimeState NOTIFY changed)
    Q_PROPERTY(QString runtimeMessage READ runtimeMessage NOTIFY changed)
    Q_PROPERTY(QString lastError READ lastError NOTIFY changed)
    Q_PROPERTY(bool canRefresh READ canRefresh NOTIFY changed)
    Q_PROPERTY(bool canSetup READ canSetup NOTIFY changed)
    Q_PROPERTY(QString setupMessage READ setupMessage NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int queryLimit READ queryLimit CONSTANT)
public:
    explicit ApplicationController(RuntimePort *port = nullptr,
                                   QObject *parent = nullptr, int launchTimeoutMs = 30000,
                                   SetupPort *setupPort = nullptr, QString unavailableReason = {});
    [[nodiscard]] QVariantList applications() const;
    [[nodiscard]] int queryLimit() const;
    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &query);
    [[nodiscard]] QString runtimeState() const;
    [[nodiscard]] QString runtimeMessage() const;
    [[nodiscard]] QString lastError() const { return m_lastError; }
    [[nodiscard]] bool canRefresh() const { return !m_port.isNull(); }
    [[nodiscard]] bool canSetup() const;
    [[nodiscard]] QString setupMessage() const { return m_setupMessage; }
    Q_INVOKABLE bool setupRuntime();
    Q_INVOKABLE bool setupImages();
    [[nodiscard]] bool busy() const { return m_pending.has_value(); }
    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool launch(const QString &entryId);
Q_SIGNALS:
    void changed();
private:
    bool openSetup(SetupTarget target);
    void synchronize();
    void withdraw(QString error = {});
    void finish(const LaunchResult &result);
    [[nodiscard]] const ApplicationStatus *statusFor(const QString &entryId) const;
    [[nodiscard]] bool launchAvailable(const ApplicationStatus &status) const;
    QPointer<RuntimePort> m_port;
    QPointer<SetupPort> m_setupPort;
    QString m_setupMessage;
    RuntimeSnapshot m_snapshot;
    QindaQt::ShellLauncher::ApplicationCatalog m_catalog;
    QString m_query;
    QString m_lastError;
    QString m_lastAttachment;
    quint64 m_lastRevision = 0;
    std::optional<LaunchRequest> m_pending;
    QTimer m_deadline;
};
} // namespace QindaQt::AndroidManager
