// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringView>

#include <optional>

namespace QindaQt::Applets {

class ApiVersion final {
public:
    static constexpr int CurrentMajor = 1;
    static constexpr int CurrentMinor = 0;

    int major = CurrentMajor;
    int minor = CurrentMinor;

    [[nodiscard]] static ApiVersion current();
    [[nodiscard]] static std::optional<ApiVersion> parse(QStringView value);
    [[nodiscard]] QString toString() const;
    [[nodiscard]] bool isValid() const;

    // The manifest is the requirement; the host may implement a newer minor API.
    [[nodiscard]] bool isSupportedBy(const ApiVersion &hostVersion) const;

    bool operator==(const ApiVersion &) const = default;
};

} // namespace QindaQt::Applets
