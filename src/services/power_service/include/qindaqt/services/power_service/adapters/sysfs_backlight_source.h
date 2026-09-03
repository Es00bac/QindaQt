// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>

#include <QtCore/QFileSystemWatcher>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace QindaQt::Power::Upstream {

// AGENT-CONTRACT: The injected sysfs root is the only filesystem location this
// source ever touches, and it is read-only except for explicit
// writeBrightness() calls. Tests inject a temporary fixture tree; production
// injects the packaged default below. There is no setuid helper, no polkit
// interaction, and no fallback root: a missing or unreadable root publishes an
// honest empty device list.
inline constexpr char kSysfsBacklightDefaultRoot[] = "/sys/class/backlight";

enum class BacklightWriteStatus : quint32 {
    Succeeded = 0,
    Unsupported = 1,
    Rejected = 2,
    Failed = 3,
};

struct BacklightWriteOutcome {
    BacklightWriteStatus status = BacklightWriteStatus::Failed;
    QString reasonCode;
    QString diagnostic;
};

// Observes `/sys/class/backlight/*` device directories under an injected root
// and converts them into Power1 internal-backlight values with strict bounded
// integer parsing. Enumeration is total: one device whose max_brightness is
// unusable still appears, typed Unavailable, instead of being silently hidden.
//
// AGENT-NOTE: writes go directly to the injected root's brightness file and
// succeed only when that file is writable by this process. Power1 v1 has no
// display-brightness wire method, so no coordinator path calls
// writeBrightness() yet; it is the tested primitive the later PB-5 method and
// the logind-apply provider (ADR-0024) can build on.
class SysfsBacklightSource : public QObject {
    Q_OBJECT

public:
    explicit SysfsBacklightSource(QString rootPath, QObject *parent = nullptr);
    ~SysfsBacklightSource() override;

    void start();
    void stop();
    [[nodiscard]] const QList<InternalBacklight> &devices() const noexcept;

    // Writes `value` to the named device's brightness file. The value must be
    // within the device's observed maximum; a read-only root yields the typed
    // Failed/"backlight-read-only" truth rather than an error escalation.
    [[nodiscard]] BacklightWriteOutcome writeBrightness(const QString &opaqueId,
                                                        quint32 value);

Q_SIGNALS:
    void devicesChanged(const QList<QindaQt::Power::InternalBacklight> &devices);

private:
    void rescan();
    void emitDevices();
    [[nodiscard]] bool readBoundedInteger(const QString &directory,
                                          const QString &fileName, quint64 bound,
                                          quint32 &value) const;
    void watchDeviceDirectory(const QString &name);

    QString m_rootPath;
    QList<InternalBacklight> m_devices;
    QFileSystemWatcher *m_watcher = nullptr;
    bool m_watching = false;
};

} // namespace QindaQt::Power::Upstream
