// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QDBusServiceWatcher>
#include <QPointer>
#include <functional>
#include <optional>
#include <QString>
#include <QVariantMap>

namespace QindaQt::Apps::SettingsInput {

// One pointer or touchpad device as the input authority (KWin) presents it.
//
// AGENT-CONTRACT: `properties` holds only the KWin device properties this
// route consumes, under their exact KWin names (camelCase, for example
// "naturalScroll", "pointerAcceleration"). Capability and default properties
// ride along under KWin's own names, which are not uniform: most are
// "supports<Prop>" / "<prop>EnabledByDefault" (for example
// "supportsNaturalScroll" / "naturalScrollEnabledByDefault"), but tap
// capability has no boolean flag at all — it is the integer
// "tapFingerCount" (0 = the device cannot tap), shared by tap-to-click and
// tap-and-drag. Verified against the live org.kde.KWin.InputDevice
// interface (qinda-top, event4); do not reintroduce a
// "supportsTapToClick"/"supportsTapAndDrag"/"defaultTapToClick" name, KWin
// has never exposed them and every row gated on them renders permanently
// hidden. So consumers can hide unsupported controls and name defaults
// without a second transport round trip. Everything is a plain
// bool/double/int/string; no transport types leak into this value type.
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
// Lifetime/threading: borrowed by the model. The synchronous seam is for
// focused fakes and direct diagnostics; Settings calls the request methods.
// Production requests finish on the receiver's thread and are cancelled by
// receiver destruction. The port must outlive its borrowing model.
class PointerDevicePort : public QObject {
    Q_OBJECT
public:
    using DevicesReply = std::function<void(QList<PointerDeviceSnapshot>, QString)>;
    struct WriteResult {
        bool applied = false;
        QString error;
        std::optional<PointerDeviceSnapshot> snapshot;
    };
    using WriteReply = std::function<void(WriteResult)>;

    explicit PointerDevicePort(QObject *parent = nullptr) : QObject(parent) {}
    ~PointerDevicePort() override;

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

    // The reply snapshot is authoritative even when a partial multi-property
    // write is refused. Never infer applied state from a successful Set alone.
    // Starts event observation lazily when the route becomes visible; model
    // construction must not make D-Bus calls (InputRouteComposition contract).
    virtual void setObserving(bool active);
    virtual void requestDevices(QObject *receiver, DevicesReply reply) const;
    virtual void requestWrite(QObject *receiver, const QString &deviceId,
                              const QList<QPair<QString, QVariant>> &changes,
                              WriteReply reply) const;

Q_SIGNALS:
    void inventoryChanged();
    void authorityChanged();
};

// Production adapter over KWin's org.kde.KWin input D-Bus API
// (org.kde.KWin.InputDeviceManager on /org/kde/KWin/InputDevice and
// org.kde.KWin.InputDevice device objects). KWin stays the live and
// persisted authority (ADR-0134); this adapter never writes config files.
class KWinPointerDevicePort final : public PointerDevicePort {
    Q_OBJECT
public:
    explicit KWinPointerDevicePort(QDBusConnection bus);

    [[nodiscard]] QList<PointerDeviceSnapshot>
    devices(QString *error) const override;
    [[nodiscard]] bool
    writeProperty(const QString &deviceId, const QString &property,
                  const QVariant &value, QString *error) const override;
    void setObserving(bool active) override;
    void requestDevices(QObject *receiver, DevicesReply reply) const override;
    void requestWrite(QObject *receiver, const QString &deviceId,
                      const QList<QPair<QString, QVariant>> &changes,
                      WriteReply reply) const override;

private Q_SLOTS:
    void handlePropertiesChanged(const QString &interface,
                                 const QVariantMap &changed,
                                 const QStringList &invalidated);

private:
    QDBusConnection m_bus;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    bool m_propertiesObserved = false;
};

} // namespace QindaQt::Apps::SettingsInput
