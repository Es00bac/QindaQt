// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "input_sink.h"
#include "profile_store.h"
#include <QObject>
#include <memory>

namespace QindaQt::Controllers {
struct RuntimeEnvironment {
    QString procRoot = QStringLiteral("/proc");
    // Focused SDL fixtures must not open or write an attached physical pad.
    // Production always uses the default, including before hardware connects.
    bool virtualDevicesOnly = false;
    // Where related input/js/hidraw nodes are discovered. Production always
    // uses the defaults; fixtures point them at a fake tree so a controller's
    // node paths can be matched by fake /proc FD links. Appended last so the
    // existing {procRoot, virtualDevicesOnly} aggregates keep their meaning.
    QString sysRoot = QStringLiteral("/sys");
    QString devRoot = QStringLiteral("/dev");
};
// Owns SDL controller handles, profiles, and their sampling timer on the Qt
// GUI thread. Sink must outlive this object. No background input thread can
// outlive the compositor. Read-only proc scans run on one owned, joined worker;
// invalidated ownership suppresses output until a current result returns.
// Steam closes every owned handle and SDL subsystem. A process holding a
// controller node pauses that controller only while it belongs to the
// application that owns the focused window and is not a passive web-engine
// host (ADR-0428); locked/inactive sessions also gate output. Fullscreen media
// keeps desktop control.
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
    // Names the process that owns the focused window; non-positive when there
    // is none. GUI thread only; never reads /proc. Held desktop input is
    // released, and output waits for a fresh scan, only when a non-web-engine
    // process currently holds one of this runtime's controllers; otherwise a
    // focus change leaves held input alone (ADR-0428).
    void setFocusedProcess(qint64 pid);
Q_SIGNALS:
    void changed(qulonglong revision);
private:
    class Private;
    std::unique_ptr<Private> d;
};
} // namespace QindaQt::Controllers
