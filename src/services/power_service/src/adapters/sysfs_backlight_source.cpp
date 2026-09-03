// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/sysfs_backlight_source.h>

#include "upstream_identity.h"

#include <qindaqt/services/power_protocol/power_limits.h>
#include <qindaqt/services/power_protocol/power_validation.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QRegularExpression>

namespace QindaQt::Power::Upstream {
namespace {

QString sanitizeDeviceName(const QString &name)
{
    return sanitizeText(name, kMaxNameUtf8Bytes);
}

// An unclassifiable device cannot be modeled honestly; the caller drops it.
bool kindFromTypeFile(const QString &typeText, BacklightKind &kind)
{
    const QString trimmed = typeText.trimmed();
    if (trimmed == QStringLiteral("firmware")) {
        kind = BacklightKind::Firmware;
        return true;
    }
    if (trimmed == QStringLiteral("platform")) {
        kind = BacklightKind::Platform;
        return true;
    }
    if (trimmed == QStringLiteral("raw")) {
        kind = BacklightKind::Raw;
        return true;
    }
    return false;
}

} // namespace

SysfsBacklightSource::SysfsBacklightSource(QString rootPath, QObject *parent)
    : QObject(parent)
    , m_rootPath(std::move(rootPath))
{
}

SysfsBacklightSource::~SysfsBacklightSource()
{
    stop();
}

const QList<InternalBacklight> &SysfsBacklightSource::devices() const noexcept
{
    return m_devices;
}

void SysfsBacklightSource::start()
{
    if (m_watching) {
        return;
    }
    if (m_watcher == nullptr) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
                &SysfsBacklightSource::rescan);
    }
    m_watching = true;
    // AGENT-NOTE: sysfs does not reliably emit inotify events; on a real host
    // external brightness changes surface through the later provider's
    // observation loop. The watcher covers fixture trees (and device
    // appearance/disappearance), which is what this slice proves.
    m_watcher->addPath(m_rootPath);
    rescan();
}

void SysfsBacklightSource::stop()
{
    if (!m_watching) {
        return;
    }
    m_watching = false;
    if (m_watcher != nullptr) {
        const QStringList watchedDirectories = m_watcher->directories();
        if (!watchedDirectories.isEmpty()) {
            m_watcher->removePaths(watchedDirectories);
        }
    }
    m_devices.clear();
}

bool SysfsBacklightSource::readBoundedInteger(const QString &directory,
                                              const QString &fileName,
                                              const quint64 bound,
                                              quint32 &value) const
{
    QFile file(directory + QLatin1Char('/') + fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.read(65);
    const bool bounded = file.atEnd();
    file.close();
    if (!bounded) {
        return false;
    }
    const QString text = QString::fromUtf8(bytes).trimmed();
    static const QRegularExpression decimal(QStringLiteral("^[0-9]+$"));
    if (!decimal.match(text).hasMatch()) {
        return false;
    }
    bool converted = false;
    const qulonglong parsed = text.toULongLong(&converted);
    // Strict decimal integers only: no signs, exponents, floats, or trailing
    // garbage. Negative and oversize values are hostile input, not truth.
    if (!converted || parsed > bound) {
        return false;
    }
    value = static_cast<quint32>(parsed);
    return true;
}

void SysfsBacklightSource::watchDeviceDirectory(const QString &name)
{
    if (m_watcher != nullptr) {
        m_watcher->addPath(m_rootPath + QLatin1Char('/') + name);
    }
}

void SysfsBacklightSource::rescan()
{
    QList<InternalBacklight> devices;
    const QDir root(m_rootPath);
    const QStringList entries =
        root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString &entry : entries) {
        const QString deviceDirectory = m_rootPath + QLatin1Char('/') + entry;
        QFile typeFile(deviceDirectory + QStringLiteral("/type"));
        if (!typeFile.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray typeBytes = typeFile.read(65);
        const bool typeBounded = typeFile.atEnd();
        typeFile.close();
        if (!typeBounded) {
            continue;
        }
        const QString typeText = QString::fromUtf8(typeBytes);
        BacklightKind kind = BacklightKind::Firmware;
        if (!kindFromTypeFile(typeText, kind)) {
            continue;
        }

        InternalBacklight device;
        device.deviceName = sanitizeDeviceName(entry);
        device.internal = true;
        device.kind = kind;
        device.handle.opaqueId = deriveOpaqueId(
            QStringLiteral("internal-backlight"), entry);
        device.status = BacklightStatus::Unavailable;
        device.reason = BacklightReason::NoBacklight;

        quint32 maximum = 0;
        if (!readBoundedInteger(deviceDirectory, QStringLiteral("max_brightness"),
                                kMaximumRawBrightness, maximum)
            || maximum == 0) {
            device.maximum = 0;
            device.observedKnown = false;
            device.diagnostic = QStringLiteral("max-brightness-unusable");
            devices.push_back(std::move(device));
            watchDeviceDirectory(entry);
            continue;
        }
        device.maximum = maximum;

        quint32 observed = 0;
        bool observedValid =
            readBoundedInteger(deviceDirectory, QStringLiteral("actual_brightness"),
                               kMaximumRawBrightness, observed);
        if (!observedValid) {
            observedValid = readBoundedInteger(
                deviceDirectory, QStringLiteral("brightness"),
                kMaximumRawBrightness, observed);
        }
        if (observedValid && observed <= maximum) {
            device.observedKnown = true;
            device.observed = observed;
            const QFileInfo brightness(deviceDirectory
                                       + QStringLiteral("/brightness"));
            if (brightness.isWritable()) {
                device.status = BacklightStatus::Ok;
                device.reason = BacklightReason::None;
            } else {
                device.status = BacklightStatus::Unavailable;
                device.reason = BacklightReason::LogindError;
                device.diagnostic = QStringLiteral("backlight-read-only");
            }
        } else {
            device.observedKnown = false;
            device.observed = 0;
            device.status = BacklightStatus::Degraded;
            device.reason = BacklightReason::DeviceDisappeared;
            device.diagnostic = observedValid
                ? QStringLiteral("observed-exceeds-maximum")
                : QStringLiteral("current-brightness-unreadable");
        }
        devices.push_back(std::move(device));
        watchDeviceDirectory(entry);
    }

    m_devices = std::move(devices);
    emitDevices();
}

void SysfsBacklightSource::emitDevices()
{
    Q_EMIT devicesChanged(m_devices);
}

BacklightWriteOutcome SysfsBacklightSource::writeBrightness(const QString &opaqueId,
                                                            const quint32 value)
{
    const InternalBacklight *target = nullptr;
    for (const InternalBacklight &device : m_devices) {
        if (device.handle.opaqueId == opaqueId) {
            target = &device;
            break;
        }
    }
    if (target == nullptr) {
        return {.status = BacklightWriteStatus::Rejected,
                .reasonCode = QStringLiteral("unknown-device"),
                .diagnostic = {}};
    }
    if (target->maximum == 0) {
        return {.status = BacklightWriteStatus::Unsupported,
                .reasonCode = QStringLiteral("backlight-unavailable"),
                .diagnostic = target->diagnostic};
    }
    if (value > target->maximum) {
        return {.status = BacklightWriteStatus::Rejected,
                .reasonCode = QStringLiteral("value-out-of-range"),
                .diagnostic = {}};
    }

    const QString directory = m_rootPath + QLatin1Char('/') + target->deviceName;
    QFile file(directory + QStringLiteral("/brightness"));
    // AGENT-GUARD: writes never escalate privileges and never fall back to
    // another path; a denied write is truthful unavailable, not an error to
    // route around.
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {.status = BacklightWriteStatus::Failed,
                .reasonCode = QStringLiteral("backlight-read-only"),
                .diagnostic = file.errorString()};
    }
    if (file.write(QString::number(value).toUtf8() + QByteArrayLiteral("\n")) < 0) {
        file.close();
        return {.status = BacklightWriteStatus::Failed,
                .reasonCode = QStringLiteral("backlight-read-only"),
                .diagnostic = file.errorString()};
    }
    file.close();

    // actual_brightness is authoritative when present. Re-read the fixture or
    // kernel view after the write instead of claiming that the request itself
    // is an observation.
    rescan();
    return {.status = BacklightWriteStatus::Succeeded,
            .reasonCode = QStringLiteral("applied"),
            .diagnostic = {}};
}

} // namespace QindaQt::Power::Upstream
