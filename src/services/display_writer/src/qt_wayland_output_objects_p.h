// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_writer/output_management_port.h>

#include "qwayland-kde-output-device-v2.h"
#include "qwayland-kde-output-management-v2.h"

#include <functional>
#include <memory>
#include <vector>

namespace QindaQt::DisplayWriter::Private
{

class DeviceMode final : public QtWayland::kde_output_device_mode_v2
{
public:
    explicit DeviceMode(::kde_output_device_mode_v2 *object)
        : QtWayland::kde_output_device_mode_v2(object)
    {
    }

    ~DeviceMode() override
    {
        if (object() != nullptr) {
            kde_output_device_mode_v2_destroy(object());
        }
    }

    [[nodiscard]] bool matches(const ModeReference &reference) const noexcept
    {
        return m_size == reference.pixelSize
            && m_refreshMilliHertz == reference.refreshMilliHertz;
    }

    [[nodiscard]] bool removed() const noexcept { return m_removed; }

protected:
    void kde_output_device_mode_v2_size(const int32_t width,
                                        const int32_t height) override
    {
        m_size = QSize(width, height);
    }

    void kde_output_device_mode_v2_refresh(const int32_t refresh) override
    {
        m_refreshMilliHertz = refresh > 0 ? static_cast<quint32>(refresh) : 0;
    }

    void kde_output_device_mode_v2_removed() override { m_removed = true; }

private:
    QSize m_size;
    quint32 m_refreshMilliHertz = 0;
    bool m_removed = false;
};

class OutputDevice final : public QtWayland::kde_output_device_v2
{
public:
    explicit OutputDevice(const quint32 globalName)
        : m_globalName(globalName)
    {
    }

    ~OutputDevice() override
    {
        m_modes.clear();
        if (object() != nullptr) {
            kde_output_device_v2_destroy(object());
        }
    }

    [[nodiscard]] quint32 globalName() const noexcept { return m_globalName; }
    [[nodiscard]] const QString &connectorName() const noexcept { return m_name; }
    [[nodiscard]] const QString &uuid() const noexcept { return m_uuid; }
    [[nodiscard]] bool ready() const noexcept { return m_ready; }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    [[nodiscard]] DeviceMode *mode(const ModeReference &reference) const
    {
        for (const auto &mode : m_modes) {
            if (!mode->removed() && mode->matches(reference)) {
                return mode.get();
            }
        }
        return nullptr;
    }

    std::function<void()> doneCallback;

protected:
    void kde_output_device_v2_mode(::kde_output_device_mode_v2 *mode) override
    {
        m_modes.push_back(std::make_unique<DeviceMode>(mode));
    }

    void kde_output_device_v2_done() override
    {
        m_ready = !m_name.isEmpty() && !m_uuid.isEmpty();
        if (doneCallback) {
            doneCallback();
        }
    }

    void kde_output_device_v2_name(const QString &name) override { m_name = name; }
    void kde_output_device_v2_uuid(const QString &uuid) override { m_uuid = uuid; }
    void kde_output_device_v2_enabled(const int32_t enabled) override
    {
        m_enabled = enabled == 1;
    }

private:
    quint32 m_globalName = 0;
    QString m_name;
    QString m_uuid;
    std::vector<std::unique_ptr<DeviceMode>> m_modes;
    bool m_ready = false;
    bool m_enabled = false;
};

class Management final : public QtWayland::kde_output_management_v2
{
public:
    ~Management() override
    {
        if (object() != nullptr) {
            kde_output_management_v2_destroy(object());
        }
    }
};

} // namespace QindaQt::DisplayWriter::Private
