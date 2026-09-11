// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace QindaQt::Apps::SettingsInput {

// Process-lifetime QML singleton composition. It owns the injected ports'
// production adapters (KWin input D-Bus, kcminputrc/kxkbrc KConfig files,
// kglobalaccel D-Bus) and the route models over them. No D-Bus call or file
// write happens in construction: the desktop may be unreachable and the
// route must still construct (ADR-0134).
//
// AGENT-CONTRACT: This is the only place that names production transports.
// Models see the port interfaces; QML sees the models. The catalog path and
// the config locations derive from the injected constructor environment so
// tests redirect XDG homes without touching the user's real files.
class InputRouteComposition final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QObject *pointerDevices READ pointerDevices CONSTANT)
    Q_PROPERTY(QObject *keyboard READ keyboard CONSTANT)
    Q_PROPERTY(QObject *layouts READ layouts CONSTANT)
    Q_PROPERTY(QObject *shortcuts READ shortcuts CONSTANT)

public:
    explicit InputRouteComposition(QObject *parent = nullptr);
    ~InputRouteComposition() override;

    [[nodiscard]] QObject *pointerDevices() const;
    [[nodiscard]] QObject *keyboard() const;
    [[nodiscard]] QObject *layouts() const;
    [[nodiscard]] QObject *shortcuts() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsInput
