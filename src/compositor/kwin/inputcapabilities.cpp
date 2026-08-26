// SPDX-License-Identifier: GPL-3.0-or-later
#include "inputcapabilities.h"

#include <core/inputdevice.h>

#include <QJsonArray>
#include <QSet>

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void addCapability(QStringList &capabilities, bool available, const QString &name)
{
    if (available) {
        capabilities.append(name);
    }
}

QStringList orderedAggregateCapabilities(const QList<InputDeviceDescriptor> &devices)
{
    QSet<QString> aggregate;
    for (const auto &device : devices) {
        for (const auto &capability : device.capabilities) {
            aggregate.insert(capability);
        }
    }

    QStringList result;
    for (const auto &capability : {QStringLiteral("keyboard"),
                                   QStringLiteral("pointer"),
                                   QStringLiteral("touchpad"),
                                   QStringLiteral("touch"),
                                   QStringLiteral("tablet-tool"),
                                   QStringLiteral("tablet-pad"),
                                   QStringLiteral("tablet-mode-switch"),
                                   QStringLiteral("lid-switch")}) {
        if (aggregate.contains(capability)) {
            result.append(capability);
        }
    }
    return result;
}

} // namespace

QJsonObject InputDeviceDescriptor::toJson() const
{
    QJsonObject object{{QStringLiteral("id"), id},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("enabled"), enabled},
                       {QStringLiteral("vendorId"), static_cast<qint64>(vendorId)},
                       {QStringLiteral("productId"), static_cast<qint64>(productId)},
                       {QStringLiteral("busType"), static_cast<qint64>(busType)},
                       {QStringLiteral("capabilities"),
                        QJsonArray::fromStringList(capabilities)}};
    if (!outputName.isEmpty()) {
        object.insert(QStringLiteral("outputName"), outputName);
    }
    if (capabilities.contains(QStringLiteral("tablet-tool"))) {
        object.insert(QStringLiteral("relativeTabletTool"), tabletToolRelative);
    }
    if (capabilities.contains(QStringLiteral("tablet-pad"))) {
        object.insert(QStringLiteral("tabletPad"),
                      QJsonObject{{QStringLiteral("buttons"), tabletPadButtonCount},
                                  {QStringLiteral("dials"), tabletPadDialCount},
                                  {QStringLiteral("rings"), tabletPadRingCount},
                                  {QStringLiteral("strips"), tabletPadStripCount}});
    }
    return object;
}

InputDeviceDescriptor describeInputDevice(const KWin::InputDevice &device, QString id)
{
    InputDeviceDescriptor result;
    result.id = std::move(id);
    result.name = device.name();
    result.outputName = device.outputName();
    result.vendorId = device.vendor();
    result.productId = device.product();
    result.busType = device.busType();
    result.enabled = device.isEnabled();
    result.tabletToolRelative = device.tabletToolIsRelative();
    result.tabletPadButtonCount = device.tabletPadButtonCount();
    result.tabletPadDialCount = device.tabletPadDialCount();
    result.tabletPadRingCount = device.tabletPadRingCount();
    result.tabletPadStripCount = device.tabletPadStripCount();
    addCapability(result.capabilities, device.isKeyboard(), QStringLiteral("keyboard"));
    addCapability(result.capabilities, device.isPointer(), QStringLiteral("pointer"));
    addCapability(result.capabilities, device.isTouchpad(), QStringLiteral("touchpad"));
    addCapability(result.capabilities, device.isTouch(), QStringLiteral("touch"));
    addCapability(result.capabilities, device.isTabletTool(), QStringLiteral("tablet-tool"));
    addCapability(result.capabilities, device.isTabletPad(), QStringLiteral("tablet-pad"));
    addCapability(result.capabilities, device.isTabletModeSwitch(),
                  QStringLiteral("tablet-mode-switch"));
    addCapability(result.capabilities, device.isLidSwitch(), QStringLiteral("lid-switch"));
    return result;
}

QJsonObject inputCapabilitiesJson(bool observerActive, QList<InputDeviceDescriptor> devices)
{
    std::sort(devices.begin(), devices.end(),
              [](const InputDeviceDescriptor &left, const InputDeviceDescriptor &right) {
                  return left.id < right.id;
              });

    QJsonArray deviceArray;
    for (const auto &device : devices) {
        deviceArray.append(device.toJson());
    }
    return {{QStringLiteral("status"), QStringLiteral("ok")},
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("observerActive"), observerActive},
            {QStringLiteral("consumesEvents"), false},
            {QStringLiteral("deviceIdStability"), QStringLiteral("adapter-lifetime")},
            {QStringLiteral("eventFamilies"),
             QJsonArray{QStringLiteral("pointer"), QStringLiteral("keyboard"),
                        QStringLiteral("touch"), QStringLiteral("tablet")}},
            {QStringLiteral("capabilities"),
             QJsonArray::fromStringList(orderedAggregateCapabilities(devices))},
            {QStringLiteral("devices"), deviceArray}};
}

} // namespace QindaQt::Compositor::KWinIntegration
