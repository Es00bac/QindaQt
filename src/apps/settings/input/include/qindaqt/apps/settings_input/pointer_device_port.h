// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QList>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Apps::SettingsInput {

// One pointer or touchpad device as the input authority (KWin) presents it.
//
// AGENT-CONTRACT: `properties` holds only the KWin device properties this
// route consumes, under their exact KWin names (camelCase, for example
// "naturalScroll", "pointerAcceleration"). Capability and default properties
// ride along under the KWin names "supportsNaturalScroll" and
// "defaultNaturalScroll", so consumers can hide unsupported controls and
// name defaults without a second transport round trip. Everything is a
// plain bool/double/string; no transport types leak into this value type.
struct PointerDeviceSnapshot {
    QString deviceId; // authority object leaf, e.g. "event25"
    QString name;     // human-readable device name
    bool touchpad = false;
    bool pointer = false;
    QVariantMap properties;
};

// Port to the pointer/touchpad authority. Implementations are fail-closed:
// when the authority is unreachable or replies are malformed, methods report
// an error instead of returning partial or default-looking truth.
//
// Lifetime/threading: borrowed by the models; methods are synchronous and
// must be called on the thread owning `bus`.
class PointerDevicePort {
public:
    virtual ~PointerDevicePort();

    // Empty list without error means the authority answered and no pointer
    // or touchpad exists (a valid state this route must present). An error
    // means the authority is unreachable or a reply was malformed.
    [[nodiscard]] virtual QList<PointerDeviceSnapshot>
    devices(QString *error) const = 0;

    // Writes exactly one device property. `property` must be a writable
    // KWin property name from the closed table in the adapter (booleans and
    // the two doubles "pointerAcceleration"/"scrollFactor"); any other name,
    // a value of the wrong type, an unknown device, or an authority error
    // fails closed with a diagnostic in `error` and no partial write.
    [[nodiscard]] virtual bool
    writeProperty(const QString &deviceId, const QString &property,
                  const QVariant &value, QString *error) const = 0;
};

// Production adapter over KWin's org.kde.KWin input D-Bus API
// (org.kde.KWin.InputDeviceManager on /org/kde/KWin/InputDevice and
// org.kde.KWin.InputDevice device objects). KWin stays the live and
// persisted authority (ADR-0134); this adapter never writes config files.
class KWinPointerDevicePort final : public PointerDevicePort {
public:
    explicit KWinPointerDevicePort(QDBusConnection bus);

    [[nodiscard]] QList<PointerDeviceSnapshot>
    devices(QString *error) const override;
    [[nodiscard]] bool
    writeProperty(const QString &deviceId, const QString &property,
                  const QVariant &value, QString *error) const override;

private:
    QDBusConnection m_bus;
};

} // namespace QindaQt::Apps::SettingsInput
