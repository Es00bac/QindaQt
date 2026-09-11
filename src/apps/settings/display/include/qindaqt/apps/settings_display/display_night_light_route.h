// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsDisplay {

// Process-lifetime QML singleton for the Display route's night light section.
// It is the composition root: resolves the two owned config paths and the
// session bus, constructs the public night light ports, and hands the UI one
// model. No other component may build production night light ports.
//
// AGENT-GUARD: The route must stay warning-silent on a busless host (Main.qml
// rows run QT_FATAL_WARNINGS=1): when the session bus is not connected the
// D-Bus ports are never constructed, and the model renders fail-closed
// unavailable truth instead.
class DisplayNightLightRoute final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *model READ model CONSTANT)

public:
    explicit DisplayNightLightRoute(QObject *parent = nullptr);
    ~DisplayNightLightRoute() override;

    [[nodiscard]] QObject *model() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsDisplay
