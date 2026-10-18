// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input_sink.h"
#include "profile_store.h"
#include <QObject>
#include <memory>

namespace QindaQt::Controllers {
struct RuntimeEnvironment { QString procRoot = QStringLiteral("/proc"); };
// Owns SDL controller handles, profiles, and their sampling timer on the Qt
// GUI thread. Sink must outlive this object. No background input thread can
// outlive the compositor. Read-only proc scans run on one owned, joined worker;
// invalidated ownership suppresses output until a current result returns.
// Steam closes every owned handle and SDL subsystem;
// other consumers, locked/inactive sessions, and fullscreen apps gate output.
class ControllerRuntime final : public QObject {
    Q_OBJECT
public:
    ControllerRuntime(InputSink &sink, QString configPath, QObject *parent = nullptr,
                      RuntimeEnvironment environment = {});
    ~ControllerRuntime() override;
    void start();
    void setContext(bool allowed, QString reason);
    QString snapshot() const;
    QString apply(const QString &id, const QString &patch, quint64 expectedRevision);
    QString resetProfile(const QString &id, quint64 expectedRevision);
    void tick();
    // Asynchronously invalidate consumer ownership and request a fresh scan.
    void refreshPriority();
Q_SIGNALS:
    void changed(qulonglong revision);
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Controllers
