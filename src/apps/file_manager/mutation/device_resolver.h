// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QtGlobal>

#include <memory>
#include <optional>

namespace QindaQt::Apps::FileManager {

// A narrow injectable seam for same-device admission. Tests can simulate
// EXDEV without mounting anything; production resolves the nearest existing
// path with lstat and never probes a host mount service.
class DeviceResolver {
public:
  virtual ~DeviceResolver() = default;
  [[nodiscard]] virtual std::optional<quint64>
  deviceForPath(const QString &path) const = 0;
};

using DeviceResolverPtr = std::shared_ptr<const DeviceResolver>;

class LocalDeviceResolver final : public DeviceResolver {
public:
  [[nodiscard]] std::optional<quint64>
  deviceForPath(const QString &path) const override;
};

} // namespace QindaQt::Apps::FileManager
