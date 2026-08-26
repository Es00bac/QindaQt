// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "developmentinputprotocol.h"

#include <QObject>

#include <memory>

namespace KWin {
class InputDevice;
class InputRedirection;
}

namespace QindaQt::Compositor::KWinIntegration {

class DevelopmentInputDeviceRegistrar
{
public:
    virtual ~DevelopmentInputDeviceRegistrar() = default;

    [[nodiscard]] virtual bool isAvailable() const = 0;
    virtual void addInputDevice(KWin::InputDevice *device) = 0;
    virtual void removeInputDevice(KWin::InputDevice *device) = 0;
};

class KWinDevelopmentInputInjector final : public QObject,
                                            public DevelopmentInputSink
{
    Q_OBJECT

public:
    explicit KWinDevelopmentInputInjector(KWin::InputRedirection *input,
                                           QObject *parent = nullptr);
    // Dependency injection keeps device registration/lifetime and emitted
    // event semantics testable without constructing KWin's global workspace.
    // The injector takes ownership and performs removal before destruction.
    explicit KWinDevelopmentInputInjector(
        std::unique_ptr<DevelopmentInputDeviceRegistrar> registrar,
        QObject *parent = nullptr);
    ~KWinDevelopmentInputInjector() override;

    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] bool inject(const DevelopmentInputBatch &batch) override;

private:
    class Device;

    std::unique_ptr<DevelopmentInputDeviceRegistrar> m_registrar;
    std::unique_ptr<Device> m_device;
    bool m_registered = false;
};

} // namespace QindaQt::Compositor::KWinIntegration
