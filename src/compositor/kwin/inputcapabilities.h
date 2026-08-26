// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

namespace KWin {
class InputDevice;
}

namespace QindaQt::Compositor::KWinIntegration {

struct InputDeviceDescriptor final
{
    QString id;
    QString name;
    QString outputName;
    QStringList capabilities;
    quint32 vendorId = 0;
    quint32 productId = 0;
    quint32 busType = 0;
    int tabletPadButtonCount = 0;
    int tabletPadDialCount = 0;
    int tabletPadRingCount = 0;
    int tabletPadStripCount = 0;
    bool enabled = false;
    bool tabletToolRelative = false;

    [[nodiscard]] QJsonObject toJson() const;
};

[[nodiscard]] InputDeviceDescriptor describeInputDevice(const KWin::InputDevice &device,
                                                        QString id);
[[nodiscard]] QJsonObject inputCapabilitiesJson(bool observerActive,
                                                QList<InputDeviceDescriptor> devices);

} // namespace QindaQt::Compositor::KWinIntegration
