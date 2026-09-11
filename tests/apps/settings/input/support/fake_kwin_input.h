// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QSharedPointer>
#include <QStringList>
#include <QVariantMap>

namespace QindaQt::Tests
{

// Fake KWin input device. Qt D-Bus exposes Q_PROPERTYs through
// org.freedesktop.DBus.Properties automatically, so GetAll, typed Set, and
// read-only rejection come for free; every accepted write is recorded for
// assertions. Initial values and capability flags come from `spec`.
class FakeInputDevice final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDevice")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool pointer READ pointer CONSTANT)
    Q_PROPERTY(bool touchpad READ touchpad CONSTANT)
    Q_PROPERTY(bool keyboard READ keyboard CONSTANT)
    Q_PROPERTY(bool supportsLeftHanded READ supportsLeftHanded CONSTANT)
    Q_PROPERTY(bool supportsNaturalScroll READ supportsNaturalScroll CONSTANT)
    Q_PROPERTY(bool supportsPointerAcceleration READ
                   supportsPointerAcceleration CONSTANT)
    Q_PROPERTY(bool supportsPointerAccelerationProfileFlat READ
                   supportsPointerAccelerationProfileFlat CONSTANT)
    Q_PROPERTY(bool supportsPointerAccelerationProfileAdaptive READ
                   supportsPointerAccelerationProfileAdaptive CONSTANT)
    Q_PROPERTY(bool supportsMiddleEmulation READ supportsMiddleEmulation
                   CONSTANT)
    Q_PROPERTY(bool supportsTapToClick READ supportsTapToClick CONSTANT)
    Q_PROPERTY(bool supportsTapAndDrag READ supportsTapAndDrag CONSTANT)
    Q_PROPERTY(bool supportsDisableWhileTyping READ supportsDisableWhileTyping
                   CONSTANT)
    Q_PROPERTY(bool supportsScrollTwoFinger READ supportsScrollTwoFinger
                   CONSTANT)
    Q_PROPERTY(bool supportsScrollEdge READ supportsScrollEdge CONSTANT)
    Q_PROPERTY(bool leftHanded READ leftHanded WRITE setLeftHanded)
    Q_PROPERTY(bool naturalScroll READ naturalScroll WRITE setNaturalScroll)
    Q_PROPERTY(double pointerAcceleration READ pointerAcceleration WRITE
                   setPointerAcceleration)
    Q_PROPERTY(bool pointerAccelerationProfileFlat READ
                   pointerAccelerationProfileFlat WRITE
                   setPointerAccelerationProfileFlat)
    Q_PROPERTY(bool pointerAccelerationProfileAdaptive READ
                   pointerAccelerationProfileAdaptive WRITE
                   setPointerAccelerationProfileAdaptive)
    Q_PROPERTY(double scrollFactor READ scrollFactor WRITE setScrollFactor)
    Q_PROPERTY(bool middleEmulation READ middleEmulation WRITE
                   setMiddleEmulation)
    Q_PROPERTY(bool tapToClick READ tapToClick WRITE setTapToClick)
    Q_PROPERTY(bool tapAndDrag READ tapAndDrag WRITE setTapAndDrag)
    Q_PROPERTY(bool disableWhileTyping READ disableWhileTyping WRITE
                   setDisableWhileTyping)
    Q_PROPERTY(bool scrollTwoFinger READ scrollTwoFinger WRITE
                   setScrollTwoFinger)
    Q_PROPERTY(bool scrollEdge READ scrollEdge WRITE setScrollEdge)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled)

public:
    explicit FakeInputDevice(const QVariantMap &spec, QObject *parent = nullptr)
        : QObject(parent)
        , m_spec(spec)
        , m_id(spec.value(QStringLiteral("id")).toString())
        , m_name(spec.value(QStringLiteral("name")).toString())
        , m_pointer(spec.value(QStringLiteral("pointer")).toBool())
        , m_touchpad(spec.value(QStringLiteral("touchpad")).toBool())
        , m_keyboard(spec.value(QStringLiteral("keyboard")).toBool())
        , m_leftHanded(spec.value(QStringLiteral("leftHanded")).toBool())
        , m_naturalScroll(spec.value(QStringLiteral("naturalScroll")).toBool())
        , m_pointerAcceleration(
              spec.value(QStringLiteral("pointerAcceleration")).toDouble())
        , m_flat(spec.value(QStringLiteral("pointerAccelerationProfileFlat"))
                     .toBool())
        , m_adaptive(spec.value(
                          QStringLiteral("pointerAccelerationProfileAdaptive"))
                         .toBool())
        , m_scrollFactor(spec.value(QStringLiteral("scrollFactor")).toDouble())
        , m_middleEmulation(
              spec.value(QStringLiteral("middleEmulation")).toBool())
        , m_tapToClick(spec.value(QStringLiteral("tapToClick")).toBool())
        , m_tapAndDrag(spec.value(QStringLiteral("tapAndDrag")).toBool())
        , m_disableWhileTyping(
              spec.value(QStringLiteral("disableWhileTyping")).toBool())
        , m_scrollTwoFinger(
              spec.value(QStringLiteral("scrollTwoFinger")).toBool())
        , m_scrollEdge(spec.value(QStringLiteral("scrollEdge")).toBool())
        , m_enabled(spec.value(QStringLiteral("enabled")).toBool())
    {
    }

    // Every accepted property write in call order, for write assertions.
    QList<QPair<QString, QVariant>> writes;

    const QString &id() const { return m_id; }
    QString name() const { return m_name; }
    bool pointer() const { return m_pointer; }
    bool touchpad() const { return m_touchpad; }
    bool keyboard() const { return m_keyboard; }
    bool specFlag(const char *flag) const
    {
        return m_spec.value(QLatin1String(flag)).toBool();
    }
    bool supportsLeftHanded() const
    {
        return specFlag("supportsLeftHanded");
    }
    bool supportsNaturalScroll() const
    {
        return specFlag("supportsNaturalScroll");
    }
    bool supportsPointerAcceleration() const
    {
        return specFlag("supportsPointerAcceleration");
    }
    bool supportsPointerAccelerationProfileFlat() const
    {
        return specFlag("supportsPointerAccelerationProfileFlat");
    }
    bool supportsPointerAccelerationProfileAdaptive() const
    {
        return specFlag("supportsPointerAccelerationProfileAdaptive");
    }
    bool supportsMiddleEmulation() const
    {
        return specFlag("supportsMiddleEmulation");
    }
    bool supportsTapToClick() const { return specFlag("supportsTapToClick"); }
    bool supportsTapAndDrag() const { return specFlag("supportsTapAndDrag"); }
    bool supportsDisableWhileTyping() const
    {
        return specFlag("supportsDisableWhileTyping");
    }
    bool supportsScrollTwoFinger() const
    {
        return specFlag("supportsScrollTwoFinger");
    }
    bool supportsScrollEdge() const { return specFlag("supportsScrollEdge"); }

    bool leftHanded() const { return m_leftHanded; }
    void setLeftHanded(bool value) { record("leftHanded", value); }
    bool naturalScroll() const { return m_naturalScroll; }
    void setNaturalScroll(bool value) { record("naturalScroll", value); }
    double pointerAcceleration() const { return m_pointerAcceleration; }
    void setPointerAcceleration(double value)
    {
        record("pointerAcceleration", value);
    }
    bool pointerAccelerationProfileFlat() const { return m_flat; }
    void setPointerAccelerationProfileFlat(bool value)
    {
        record("pointerAccelerationProfileFlat", value);
    }
    bool pointerAccelerationProfileAdaptive() const { return m_adaptive; }
    void setPointerAccelerationProfileAdaptive(bool value)
    {
        record("pointerAccelerationProfileAdaptive", value);
    }
    double scrollFactor() const { return m_scrollFactor; }
    void setScrollFactor(double value) { record("scrollFactor", value); }
    bool middleEmulation() const { return m_middleEmulation; }
    void setMiddleEmulation(bool value) { record("middleEmulation", value); }
    bool tapToClick() const { return m_tapToClick; }
    void setTapToClick(bool value) { record("tapToClick", value); }
    bool tapAndDrag() const { return m_tapAndDrag; }
    void setTapAndDrag(bool value) { record("tapAndDrag", value); }
    bool disableWhileTyping() const { return m_disableWhileTyping; }
    void setDisableWhileTyping(bool value)
    {
        record("disableWhileTyping", value);
    }
    bool scrollTwoFinger() const { return m_scrollTwoFinger; }
    void setScrollTwoFinger(bool value) { record("scrollTwoFinger", value); }
    bool scrollEdge() const { return m_scrollEdge; }
    void setScrollEdge(bool value) { record("scrollEdge", value); }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool value) { record("enabled", value); }

private:
    void record(const char *property, const QVariant &value)
    {
        writes.append({QLatin1String(property), value});
        if (property == QLatin1String("leftHanded")) m_leftHanded = value.toBool();
        else if (property == QLatin1String("naturalScroll")) m_naturalScroll = value.toBool();
        else if (property == QLatin1String("pointerAcceleration")) m_pointerAcceleration = value.toDouble();
        else if (property == QLatin1String("pointerAccelerationProfileFlat")) m_flat = value.toBool();
        else if (property == QLatin1String("pointerAccelerationProfileAdaptive")) m_adaptive = value.toBool();
        else if (property == QLatin1String("scrollFactor")) m_scrollFactor = value.toDouble();
        else if (property == QLatin1String("middleEmulation")) m_middleEmulation = value.toBool();
        else if (property == QLatin1String("tapToClick")) m_tapToClick = value.toBool();
        else if (property == QLatin1String("tapAndDrag")) m_tapAndDrag = value.toBool();
        else if (property == QLatin1String("disableWhileTyping")) m_disableWhileTyping = value.toBool();
        else if (property == QLatin1String("scrollTwoFinger")) m_scrollTwoFinger = value.toBool();
        else if (property == QLatin1String("scrollEdge")) m_scrollEdge = value.toBool();
        else if (property == QLatin1String("enabled")) m_enabled = value.toBool();
    }

    QVariantMap m_spec;
    QString m_id;
    QString m_name;
    bool m_pointer;
    bool m_touchpad;
    bool m_keyboard;
    bool m_leftHanded;
    bool m_naturalScroll;
    double m_pointerAcceleration;
    bool m_flat;
    bool m_adaptive;
    double m_scrollFactor;
    bool m_middleEmulation;
    bool m_tapToClick;
    bool m_tapAndDrag;
    bool m_disableWhileTyping;
    bool m_scrollTwoFinger;
    bool m_scrollEdge;
    bool m_enabled;
};

// Manager object on one private bus, service org.kde.KWin. KWin lists
// touchpads among the pointers, so ListPointers returns both.
class FakeKWinInput final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWin.InputDeviceManager")
public:
    bool publish(QDBusConnection bus)
    {
        const auto options = QDBusConnection::ExportAllContents;
        for (const auto &device : std::as_const(m_devices)) {
            bus.registerObject(
                QStringLiteral("/org/kde/KWin/InputDevice/%1").arg(
                    device->id()),
                device.data(), options);
        }
        return bus.registerService(QStringLiteral("org.kde.KWin")) &&
               bus.registerObject(QStringLiteral("/org/kde/KWin/InputDevice"),
                                  this, options);
    }

    FakeInputDevice *addDevice(const QVariantMap &spec)
    {
        auto device = QSharedPointer<FakeInputDevice>::create(spec, this);
        m_devices.append(device);
        return device.data();
    }

    FakeInputDevice *deviceAt(int row) const { return m_devices.at(row).data(); }

public Q_SLOTS:
    Q_SCRIPTABLE QStringList ListPointers()
    {
        QStringList ids;
        for (const auto &device : std::as_const(m_devices)) {
            if (device->pointer() || device->touchpad()) {
                ids.append(device->id());
            }
        }
        return ids;
    }
    Q_SCRIPTABLE QStringList ListTouch() { return {}; }
    Q_SCRIPTABLE QStringList ListKeyboards() { return {}; }

private:
    QList<QSharedPointer<FakeInputDevice>> m_devices;
};

} // namespace QindaQt::Tests
