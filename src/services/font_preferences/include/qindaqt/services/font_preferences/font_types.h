// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringView>
#include <QtGlobal>

#include <optional>

namespace QindaQt::Services::FontPreferences {

// AGENT-CONTRACT: Font antialiasing options match the Settings1 schema values
// and map directly to QFont render hint strategies.
enum class FontAntialiasing : quint8 {
    None = 0,
    Grayscale = 1,
    Subpixel = 2
};

// AGENT-CONTRACT: Font hinting styles align with the fonts.hinting schema
// allowed values {"none", "slight", "medium", "full"}.
enum class FontHinting : quint8 {
    None = 0,
    Slight = 1,
    Medium = 2,
    Full = 3
};

// AGENT-CONTRACT: Subpixel ordering matches the fonts.subpixelOrder schema
// allowed values {"none", "rgb", "bgr", "vrgb", "vbgr"} plus Unknown fallback.
enum class FontSubpixelOrder : quint8 {
    None = 0,
    Rgb = 1,
    Bgr = 2,
    Vrgb = 3,
    Vbgr = 4,
    Unknown = 5
};

[[nodiscard]] QString fontAntialiasingToString(FontAntialiasing mode);
[[nodiscard]] std::optional<FontAntialiasing> fontAntialiasingFromString(QStringView str);
[[nodiscard]] bool fontAntialiasingToBool(FontAntialiasing mode) noexcept;
[[nodiscard]] FontAntialiasing fontAntialiasingFromBool(bool enabled) noexcept;

[[nodiscard]] QString fontHintingToString(FontHinting hinting);
[[nodiscard]] std::optional<FontHinting> fontHintingFromString(QStringView str);

[[nodiscard]] QString fontSubpixelOrderToString(FontSubpixelOrder order);
[[nodiscard]] std::optional<FontSubpixelOrder> fontSubpixelOrderFromString(QStringView str);

} // namespace QindaQt::Services::FontPreferences
