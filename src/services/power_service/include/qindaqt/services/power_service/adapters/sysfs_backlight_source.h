// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_protocol/power_types.h>
#include <qindaqt/services/power_service/adapters/backlight_writer.h>

#include <QtCore/QFileSystemWatcher>
#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

namespace QindaQt::Power::Upstream {

// AGENT-CONTRACT: The injected sysfs root is the only filesystem location this
// source ever touches, and it is read-only except for explicit
// writeBrightness() calls. Tests inject a temporary fixture tree; production
// injects the packaged default below. There is no setuid helper, no polkit
// interaction, and no fallback root: a missing or unreadable root publishes an
// honest empty device list.
inline constexpr char kSysfsBacklightDefaultRoot[] = "/sys/class/backlight";

// Observes `/sys/class/backlight/*` device directories under an injected root
// and converts them into Power1 internal-backlight values with strict bounded
// integer parsing. Enumeration is total: one device whose max_brightness is
// unusable still appears, typed Unavailable, instead of being silently hidden.
//
// AGENT-NOTE: writes go to the injected root's brightness file when this
// process can write it, and otherwise to the injected BacklightWriter, which
// delegates to a service that already holds the authority (ADR-0186). Nothing
// here escalates privileges. Power1 SetInternalBrightness reaches
// writeBrightness() only through ProductionBatteryCollaborator, after the
// shared target rule (ADR-0148); the desktop-controls media keys are a
// separate local consumer.
//
// AGENT-GUARD: this file is transport-fenced by
// tests/services/power_service/check_boundary.cmake — it must never name a
// bus, a daemon or a session. Everything privileged goes through the abstract
// writer.
class SysfsBacklightSource : public QObject {
    Q_OBJECT

public:
    explicit SysfsBacklightSource(QString rootPath, QObject *parent = nullptr);
    // Takes ownership of the privileged writer used when the kernel attribute
    // is not writable by this process. A null writer keeps the sysfs-only
    // behaviour: a read-only attribute is then published Unavailable.
    SysfsBacklightSource(QString rootPath, std::unique_ptr<BacklightWriter> writer,
                         QObject *parent = nullptr);
    ~SysfsBacklightSource() override;

    void start();
    void stop();
    [[nodiscard]] const QList<InternalBacklight> &devices() const noexcept;

    // Writes `value` for the named device. The value must be within the
    // device's observed maximum. A kernel attribute this process cannot write
    // is routed to the injected writer; with no writer, the typed
    // Failed/"backlight-read-only" truth is returned rather than an error
    // escalation.
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
    std::unique_ptr<BacklightWriter> m_writer;
    QList<InternalBacklight> m_devices;
    QFileSystemWatcher *m_watcher = nullptr;
    bool m_watching = false;
};

} // namespace QindaQt::Power::Upstream
