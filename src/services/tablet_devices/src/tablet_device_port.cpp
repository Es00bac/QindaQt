// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_device_port.h>

#include <QRegularExpression>
#include <QSet>

namespace QindaQt::Services::TabletDevices {

QVariantList TabletArea::toVariantList() const {
    return QVariantList{x, y, width, height};
}

TabletArea TabletArea::fromVariant(const QVariant &value, bool *ok) {
    const auto fail = [ok]() {
        if (ok != nullptr) {
            *ok = false;
        }
        return TabletArea{};
    };
    if (value.typeId() != QMetaType::QVariantList) {
        return fail();
    }
    const QVariantList parts = value.toList();
    if (parts.size() != 4) {
        return fail();
    }
    TabletArea area;
    double *members[4] = {&area.x, &area.y, &area.width, &area.height};
    for (int index = 0; index < 4; ++index) {
        const QVariant &part = parts.at(index);
        if (!part.canConvert<double>() ||
            part.typeId() == QMetaType::QString) {
            return fail();
        }
        *members[index] = part.toDouble();
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return area;
}

double TabletDeviceSnapshot::widthMillimeters() const {
    // KWin's `size` is a `(dd)` pair in millimetres, carried here as a
    // two-element list; -1,-1 means the device reports no physical size.
    const QVariant raw = properties.value(QStringLiteral("size"));
    if (raw.typeId() != QMetaType::QVariantList) {
        return -1.0;
    }
    const QVariantList parts = raw.toList();
    return parts.size() == 2 ? parts.at(0).toDouble() : -1.0;
}

double TabletDeviceSnapshot::heightMillimeters() const {
    const QVariant raw = properties.value(QStringLiteral("size"));
    if (raw.typeId() != QMetaType::QVariantList) {
        return -1.0;
    }
    const QVariantList parts = raw.toList();
    return parts.size() == 2 ? parts.at(1).toDouble() : -1.0;
}

bool TabletDeviceSnapshot::hasPhysicalSize() const {
    return widthMillimeters() > 0.0 && heightMillimeters() > 0.0;
}

TabletDevicePort::~TabletDevicePort() = default;

TabletDeviceWatcher::TabletDeviceWatcher(QObject *parent) : QObject(parent) {}

TabletDeviceWatcher::~TabletDeviceWatcher() = default;

bool isWritableTabletProperty(const QString &property) {
    // AGENT-GUARD: The writable-property table is closed. A name outside it
    // must never reach D-Bus: KWin accepts unknown writes on some paths and
    // the route would then present unobserved truth as a change.
    static const QSet<QString> writable{
        QStringLiteral("enabled"),
        QStringLiteral("outputName"),
        QStringLiteral("mapToWorkspace"),
        QStringLiteral("outputArea"),
        QStringLiteral("inputArea"),
        QStringLiteral("calibrationMatrix"),
        QStringLiteral("pressureCurve"),
        QStringLiteral("pressureRangeMin"),
        QStringLiteral("pressureRangeMax"),
        QStringLiteral("rotation"),
        QStringLiteral("leftHanded"),
        QStringLiteral("tabletToolIsRelative"),
    };
    return writable.contains(property);
}

QString tabletBaseName(const QString &deviceName) {
    QString name = deviceName.trimmed();
    for (const QLatin1String suffix :
         {QLatin1String(" Pen"), QLatin1String(" Pad"),
          QLatin1String(" Finger"), QLatin1String(" Touch"),
          QLatin1String(" Stylus"), QLatin1String(" Eraser")}) {
        if (name.endsWith(suffix)) {
            return name.left(name.size() - suffix.size()).trimmed();
        }
    }
    return name;
}

QString tabletIdentity(const TabletDeviceSnapshot &device) {
    const QString base = tabletBaseName(device.name);
    if (base.isEmpty() && device.vendorId == 0 && device.productId == 0) {
        // Nothing stable to key on. Returning empty is what stops the policy
        // recording a decision it could never find again.
        return {};
    }
    return QStringLiteral("%1:%2:%3")
        .arg(device.vendorId)
        .arg(device.productId)
        .arg(base);
}

bool isLegacyPointerHashIdentity(const QString &key) {
    // KWin's deviceGroupId is base64 of a SHA-1: 28 characters ending in '='.
    // The stable identity always contains two ':' separators and starts with
    // a digit, so the two forms cannot be confused.
    static const QRegularExpression base64Sha1(
        QStringLiteral("^[A-Za-z0-9+/]{27}=$"));
    return base64Sha1.match(key).hasMatch();
}

} // namespace QindaQt::Services::TabletDevices
