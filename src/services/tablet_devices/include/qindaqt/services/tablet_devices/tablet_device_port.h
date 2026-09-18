// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Services::TabletDevices {

// A normalized fractional rectangle as KWin's `(dddd)` area properties carry
// it: origin and extent in 0..1 of the mapped surface. Kept as a plain value
// so no transport type reaches the models (the adapter converts).
struct TabletArea {
    double x = 0.0;
    double y = 0.0;
    double width = 1.0;
    double height = 1.0;

    [[nodiscard]] bool isWhole() const noexcept {
        return x == 0.0 && y == 0.0 && width == 1.0 && height == 1.0;
    }
    // A usable area is inside the unit square and has a positive extent;
    // KWin rejects anything else and so does this port.
    [[nodiscard]] bool isValid() const noexcept {
        return width > 0.0 && height > 0.0 && x >= 0.0 && y >= 0.0 &&
               x + width <= 1.0 + 1e-9 && y + height <= 1.0 + 1e-9;
    }
    [[nodiscard]] QVariantList toVariantList() const;
    [[nodiscard]] static TabletArea fromVariant(const QVariant &value,
                                                bool *ok = nullptr);

    friend bool operator==(const TabletArea &, const TabletArea &) = default;
};

// One tablet tool or tablet pad as the input authority (KWin) presents it.
//
// AGENT-CONTRACT: `properties` holds only the KWin device properties this
// feature consumes, under their exact KWin names (camelCase, for example
// "outputName", "pressureCurve"). Capability flags ride along under the KWin
// names ("supportsCalibrationMatrix", "supportsRotation", …) and defaults
// under theirs ("defaultPressureCurve", …), so consumers can hide
// unsupported controls and offer a reset without a second round trip.
// Values are bool/double/uint/QString or, for the two area properties, a
// four-element QVariantList of doubles. No transport type leaks out.
struct TabletDeviceSnapshot {
    QString deviceId; // authority object leaf, e.g. "event19"
    QString name;     // human-readable device name
    // AGENT-GUARD: KWin derives `deviceGroupId` from the ADDRESS of the
    // libinput device group object
    // (kwin-6.6.6 src/backends/libinput/device.cpp:450 hashes
    // `QString::asprintf("%p", group)`). It is stable only while that object
    // lives, so it is useful for pairing a pen with its pad inside ONE
    // session and must never key anything that outlives a re-plug. Use
    // tabletIdentity() for that.
    QString deviceGroupId;
    quint32 vendorId = 0;
    quint32 productId = 0;
    bool tabletTool = false;
    bool tabletPad = false;
    QVariantMap properties;

    // Physical tablet size in millimetres when the authority reports one;
    // width <= 0 means unknown (KWin reports -1,-1 for devices without it).
    [[nodiscard]] double widthMillimeters() const;
    [[nodiscard]] double heightMillimeters() const;
    [[nodiscard]] bool hasPhysicalSize() const;
};

// Port to the tablet authority. Implementations are fail-closed: when the
// authority is unreachable or a reply is malformed, methods report an error
// instead of returning partial or default-looking truth.
//
// Lifetime/threading: borrowed by the models and the session policy; methods
// are synchronous and must be called on the thread owning the transport.
class TabletDevicePort {
public:
    virtual ~TabletDevicePort();

    // Empty list without error means the authority answered and no tablet
    // exists (a valid state the route must present). An error means the
    // authority is unreachable or a reply was malformed.
    [[nodiscard]] virtual QList<TabletDeviceSnapshot>
    devices(QString *error) const = 0;

    // Reads exactly one device. Returning false with an EMPTY error means
    // the device is absent or is not a tablet — both are ordinary states a
    // listing skips. Returning false with a diagnostic means the authority
    // is unreachable or its reply was uninterpretable.
    [[nodiscard]] virtual bool device(const QString &deviceId,
                                      TabletDeviceSnapshot *snapshot,
                                      QString *error) const = 0;

    // Writes exactly one device property. `property` must be a writable KWin
    // property name from the closed table in the adapter; any other name, a
    // value of the wrong type, an unknown device, or an authority error
    // fails closed with a diagnostic in `error` and no partial write.
    [[nodiscard]] virtual bool writeProperty(const QString &deviceId,
                                             const QString &property,
                                             const QVariant &value,
                                             QString *error) const = 0;
};

// Hotplug notifications from the tablet authority. Separate from the port so
// the session policy can subscribe without the route paying for signals and
// so tests can drive arrivals without a bus.
class TabletDeviceWatcher : public QObject {
    Q_OBJECT
public:
    explicit TabletDeviceWatcher(QObject *parent = nullptr);
    ~TabletDeviceWatcher() override;

    // Begins observing. Returns false with a diagnostic when the authority's
    // signals cannot be observed; the caller must then treat hotplug as
    // unavailable rather than assume no device ever arrives.
    [[nodiscard]] virtual bool start(QString *error) = 0;

Q_SIGNALS:
    // `deviceId` is the authority object leaf, not a path.
    void deviceAdded(const QString &deviceId);
    void deviceRemoved(const QString &deviceId);
};

// The closed set of writable tablet properties, exported so tests and the
// route can assert the boundary rather than re-declare it.
[[nodiscard]] bool isWritableTabletProperty(const QString &property);

// The identity a remembered decision is keyed on: `vendor:product:name`,
// with the trailing role word removed so a pen and its pad are one tablet.
//
// AGENT-CONTRACT: this is KWin's OWN per-device key. KWin persists every
// device setting under
//   group("Libinput").group(vendor).group(product).group(name)
// (kwin-6.6.6 src/backends/libinput/connection.cpp:716), which is the
// `[Libinput][1386][934][Wacom One Pen Display 13 Pen]` group in
// `kcminputrc`. Keying the ledger the same way makes QindaQt's record and
// KWin's own persistence agree by construction across unplug, re-plug and
// login.
//
// Two identical tablets of the same model share this identity, exactly as
// they share KWin's config group. That is a known and documented limit, not
// an oversight: libinput exposes no serial through KWin's D-Bus surface.
[[nodiscard]] QString tabletIdentity(const TabletDeviceSnapshot &device);

// The tablet's name with the trailing role word ("Pen", "Pad", "Finger",
// "Touch") removed — what a user calls the tablet.
[[nodiscard]] QString tabletBaseName(const QString &deviceName);

// True when `key` looks like a ledger key written before the identity was
// stable: KWin's base64 SHA-1 of a pointer address. Such a record can never
// match a live device again, so it is dropped rather than left as a phantom.
[[nodiscard]] bool isLegacyPointerHashIdentity(const QString &key);

} // namespace QindaQt::Services::TabletDevices
