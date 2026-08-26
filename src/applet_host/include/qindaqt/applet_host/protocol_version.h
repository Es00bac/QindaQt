// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringView>

#include <optional>

namespace QindaQt::AppletHost {

class ProtocolVersion final {
public:
    static constexpr int CurrentMajor = 1;
    static constexpr int CurrentMinor = 0;

    int major = CurrentMajor;
    int minor = CurrentMinor;

    [[nodiscard]] static ProtocolVersion current();
    [[nodiscard]] static std::optional<ProtocolVersion> parse(QStringView value);
    [[nodiscard]] QString toString() const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] std::optional<ProtocolVersion> negotiate(
        const ProtocolVersion &peer) const;

    bool operator==(const ProtocolVersion &) const = default;
};

} // namespace QindaQt::AppletHost
