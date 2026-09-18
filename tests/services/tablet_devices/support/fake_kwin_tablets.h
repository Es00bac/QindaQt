// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QRectF>
#include <QSharedPointer>
#include <QStringList>
#include <QVariantMap>

namespace QindaQt::Tests {

// Fake KWin input device with the tablet property surface KWin 6.6.6
// advertises (verified by introspecting org.kde.KWin.InputDevice on
// qinda-top, 2026-09-17). Qt exposes Q_PROPERTYs through
// org.freedesktop.DBus.Properties automatically, so GetAll, typed Set and
// read-only rejection come for free; every accepted write is recorded.
class FakeTabletDevice final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDevice")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString sysName READ id CONSTANT)
    Q_PROPERTY(QString deviceGroupId READ deviceGroupId CONSTANT)
    Q_PROPERTY(uint vendor READ vendor CONSTANT)
    Q_PROPERTY(uint product READ product CONSTANT)
    Q_PROPERTY(bool tabletTool READ tabletTool CONSTANT)
    Q_PROPERTY(bool tabletPad READ tabletPad CONSTANT)
    Q_PROPERTY(bool pointer READ pointer CONSTANT)
    Q_PROPERTY(QSizeF size READ size CONSTANT)
    Q_PROPERTY(uint tabletPadButtonCount READ padButtons CONSTANT)
    Q_PROPERTY(uint tabletPadRingCount READ padRings CONSTANT)
    Q_PROPERTY(uint tabletPadStripCount READ padStrips CONSTANT)
    Q_PROPERTY(uint tabletPadDialCount READ padDials CONSTANT)
    Q_PROPERTY(bool supportsDisableEvents READ supportsDisableEvents CONSTANT)
    Q_PROPERTY(bool supportsOutputArea READ supportsOutputArea CONSTANT)
    Q_PROPERTY(bool supportsInputArea READ supportsInputArea CONSTANT)
    Q_PROPERTY(bool supportsCalibrationMatrix READ supportsCalibrationMatrix
                   CONSTANT)
    Q_PROPERTY(bool supportsPressureRange READ supportsPressureRange CONSTANT)
    Q_PROPERTY(bool supportsRotation READ supportsRotation CONSTANT)
    Q_PROPERTY(bool supportsLeftHanded READ supportsLeftHanded CONSTANT)
    Q_PROPERTY(QString defaultCalibrationMatrix READ defaultCalibrationMatrix
                   CONSTANT)
    Q_PROPERTY(QString defaultPressureCurve READ defaultPressureCurve CONSTANT)
    Q_PROPERTY(double defaultPressureRangeMin READ defaultPressureRangeMin
                   CONSTANT)
    Q_PROPERTY(double defaultPressureRangeMax READ defaultPressureRangeMax
                   CONSTANT)
    Q_PROPERTY(uint defaultRotation READ defaultRotation CONSTANT)
    Q_PROPERTY(bool defaultMapToWorkspace READ defaultMapToWorkspace CONSTANT)
    Q_PROPERTY(QRectF defaultOutputArea READ defaultOutputArea CONSTANT)
    Q_PROPERTY(QRectF defaultInputArea READ defaultInputArea CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled)
    Q_PROPERTY(QString outputName READ outputName WRITE setOutputName)
    Q_PROPERTY(bool mapToWorkspace READ mapToWorkspace WRITE setMapToWorkspace)
    Q_PROPERTY(QRectF outputArea READ outputArea WRITE setOutputArea)
    Q_PROPERTY(QRectF inputArea READ inputArea WRITE setInputArea)
    Q_PROPERTY(QString calibrationMatrix READ calibrationMatrix WRITE
                   setCalibrationMatrix)
    Q_PROPERTY(QString pressureCurve READ pressureCurve WRITE setPressureCurve)
    Q_PROPERTY(double pressureRangeMin READ pressureRangeMin WRITE
                   setPressureRangeMin)
    Q_PROPERTY(double pressureRangeMax READ pressureRangeMax WRITE
                   setPressureRangeMax)
    Q_PROPERTY(uint rotation READ rotation WRITE setRotation)
    Q_PROPERTY(bool leftHanded READ leftHanded WRITE setLeftHanded)
    Q_PROPERTY(bool tabletToolIsRelative READ relative WRITE setRelative)

public:
    explicit FakeTabletDevice(const QVariantMap &spec, QObject *parent = nullptr)
        : QObject(parent), m_spec(spec),
          m_id(spec.value(QStringLiteral("id")).toString()),
          m_outputName(spec.value(QStringLiteral("outputName")).toString()),
          m_calibration(
              spec.value(QStringLiteral("calibrationMatrix"),
                         QStringLiteral("1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1"))
                  .toString()),
          m_pressureCurve(spec.value(QStringLiteral("pressureCurve"),
                                     QStringLiteral("0,0;1,1;"))
                              .toString()) {}

    // Every accepted property write in call order, for write assertions.
    QList<QPair<QString, QVariant>> writes;

    QString id() const { return m_id; }
    QString name() const { return m_spec.value(QStringLiteral("name")).toString(); }
    QString deviceGroupId() const {
        return m_spec.value(QStringLiteral("deviceGroupId")).toString();
    }
    uint vendor() const { return m_spec.value(QStringLiteral("vendor")).toUInt(); }
    uint product() const {
        return m_spec.value(QStringLiteral("product")).toUInt();
    }
    bool flag(const char *key) const {
        return m_spec.value(QLatin1String(key)).toBool();
    }
    bool tabletTool() const { return flag("tabletTool"); }
    bool tabletPad() const { return flag("tabletPad"); }
    bool pointer() const { return flag("pointer"); }
    QSizeF size() const {
        return m_spec.contains(QStringLiteral("widthMm"))
                   ? QSizeF(m_spec.value(QStringLiteral("widthMm")).toDouble(),
                            m_spec.value(QStringLiteral("heightMm")).toDouble())
                   : QSizeF(-1, -1);
    }
    uint padCount(const char *key) const {
        return m_spec.contains(QLatin1String(key))
                   ? m_spec.value(QLatin1String(key)).toUInt()
                   : 0xFFFFFFFFU;
    }
    uint padButtons() const { return padCount("padButtons"); }
    uint padRings() const { return padCount("padRings"); }
    uint padStrips() const { return padCount("padStrips"); }
    uint padDials() const { return padCount("padDials"); }
    bool supportsDisableEvents() const { return flag("supportsDisableEvents"); }
    bool supportsOutputArea() const { return flag("supportsOutputArea"); }
    bool supportsInputArea() const { return flag("supportsInputArea"); }
    bool supportsCalibrationMatrix() const {
        return flag("supportsCalibrationMatrix");
    }
    bool supportsPressureRange() const { return flag("supportsPressureRange"); }
    bool supportsRotation() const { return flag("supportsRotation"); }
    bool supportsLeftHanded() const { return flag("supportsLeftHanded"); }
    QString defaultCalibrationMatrix() const {
        return QStringLiteral("1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1");
    }
    QString defaultPressureCurve() const { return QStringLiteral("0,0;1,1;"); }
    double defaultPressureRangeMin() const { return 0.0; }
    double defaultPressureRangeMax() const { return 1.0; }
    uint defaultRotation() const { return 0; }
    bool defaultMapToWorkspace() const { return false; }
    QRectF defaultOutputArea() const { return QRectF(0, 0, 1, 1); }
    QRectF defaultInputArea() const { return QRectF(0, 0, 1, 1); }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool value) { record("enabled", value, m_enabled, value); }
    QString outputName() const { return m_outputName; }
    void setOutputName(const QString &value) {
        record("outputName", value, m_outputName, value);
    }
    bool mapToWorkspace() const { return m_workspace; }
    void setMapToWorkspace(bool value) {
        record("mapToWorkspace", value, m_workspace, value);
    }
    QRectF outputArea() const { return m_outputArea; }
    void setOutputArea(const QRectF &value) {
        record("outputArea", value, m_outputArea, value);
    }
    QRectF inputArea() const { return m_inputArea; }
    void setInputArea(const QRectF &value) {
        record("inputArea", value, m_inputArea, value);
    }
    QString calibrationMatrix() const { return m_calibration; }
    void setCalibrationMatrix(const QString &value) {
        record("calibrationMatrix", value, m_calibration, value);
    }
    QString pressureCurve() const { return m_pressureCurve; }
    void setPressureCurve(const QString &value) {
        record("pressureCurve", value, m_pressureCurve, value);
    }
    double pressureRangeMin() const { return m_pressureMin; }
    void setPressureRangeMin(double value) {
        record("pressureRangeMin", value, m_pressureMin, value);
    }
    double pressureRangeMax() const { return m_pressureMax; }
    void setPressureRangeMax(double value) {
        record("pressureRangeMax", value, m_pressureMax, value);
    }
    uint rotation() const { return m_rotation; }
    void setRotation(uint value) { record("rotation", value, m_rotation, value); }
    bool leftHanded() const { return m_leftHanded; }
    void setLeftHanded(bool value) {
        record("leftHanded", value, m_leftHanded, value);
    }
    bool relative() const { return m_relative; }
    void setRelative(bool value) {
        record("tabletToolIsRelative", value, m_relative, value);
    }

private:
    template <typename T, typename U>
    void record(const char *property, const QVariant &wire, T &member,
                const U &value) {
        writes.append({QLatin1String(property), wire});
        member = value;
    }

    QVariantMap m_spec;
    QString m_id;
    QString m_outputName;
    QString m_calibration;
    QString m_pressureCurve;
    QRectF m_outputArea{0, 0, 1, 1};
    QRectF m_inputArea{0, 0, 1, 1};
    double m_pressureMin = 0.0;
    double m_pressureMax = 1.0;
    uint m_rotation = 0;
    bool m_enabled = true;
    bool m_workspace = false;
    bool m_leftHanded = false;
    bool m_relative = false;
};

// Manager object on one private bus, service org.kde.KWin. KWin 6.6 has no
// ListTablets, so the port reads `devicesSysNames` and filters; the fake
// publishes exactly that plus the hotplug signals.
class FakeKWinTabletManager final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDeviceManager")
    Q_PROPERTY(QStringList devicesSysNames READ devicesSysNames)

public:
    bool publish(QDBusConnection bus) {
        m_bus = bus;
        const auto options = QDBusConnection::ExportAllContents;
        for (const auto &device : std::as_const(m_devices)) {
            bus.registerObject(QStringLiteral("/org/kde/KWin/InputDevice/%1")
                                   .arg(device->id()),
                               device.data(), options);
        }
        return bus.registerService(QStringLiteral("org.kde.KWin")) &&
               bus.registerObject(QStringLiteral("/org/kde/KWin/InputDevice"),
                                  this, options);
    }

    FakeTabletDevice *addDevice(const QVariantMap &spec) {
        auto device = QSharedPointer<FakeTabletDevice>::create(spec, this);
        m_devices.append(device);
        if (m_bus.isConnected()) {
            m_bus.registerObject(QStringLiteral("/org/kde/KWin/InputDevice/%1")
                                     .arg(device->id()),
                                 device.data(),
                                 QDBusConnection::ExportAllContents);
            emitDeviceAdded(device->id());
        }
        return device.data();
    }

    void removeDevice(const QString &id) {
        for (qsizetype index = 0; index < m_devices.size(); ++index) {
            if (m_devices.at(index)->id() != id) {
                continue;
            }
            m_devices.removeAt(index);
            if (m_bus.isConnected()) {
                m_bus.unregisterObject(
                    QStringLiteral("/org/kde/KWin/InputDevice/%1").arg(id));
                emitDeviceRemoved(id);
            }
            return;
        }
    }

    FakeTabletDevice *deviceAt(int row) const {
        return m_devices.at(row).data();
    }

    QStringList devicesSysNames() const {
        QStringList names;
        for (const auto &device : std::as_const(m_devices)) {
            names.append(device->id());
        }
        return names;
    }

    void emitDeviceAdded(const QString &id) {
        auto message = QDBusMessage::createSignal(
            QStringLiteral("/org/kde/KWin/InputDevice"),
            QStringLiteral("org.kde.KWin.InputDeviceManager"),
            QStringLiteral("deviceAdded"));
        message.setArguments({id});
        m_bus.send(message);
    }

    void emitDeviceRemoved(const QString &id) {
        auto message = QDBusMessage::createSignal(
            QStringLiteral("/org/kde/KWin/InputDevice"),
            QStringLiteral("org.kde.KWin.InputDeviceManager"),
            QStringLiteral("deviceRemoved"));
        message.setArguments({id});
        m_bus.send(message);
    }

private:
    QList<QSharedPointer<FakeTabletDevice>> m_devices;
    QDBusConnection m_bus{QStringLiteral("invalid")};
};

} // namespace QindaQt::Tests
