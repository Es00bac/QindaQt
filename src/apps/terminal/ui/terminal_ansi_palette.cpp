// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_ansi_palette.h"
#include "qindaqt/design_tokens/token_deriver.h"

namespace QindaQt::Apps::Terminal {
namespace {
using QindaQt::DesignTokens::DesignTokenDeriver;

bool useLightInk(const QColor &background) {
  return DesignTokenDeriver::contrastRatio(Qt::white, background) >=
         DesignTokenDeriver::contrastRatio(Qt::black, background);
}
} // namespace

QColor terminalReadableColor(QColor color, const QColor &background,
                             double minimumContrast) {
  color.setAlpha(255);
  if (DesignTokenDeriver::contrastRatio(color, background) >= minimumContrast)
    return color;
  const double endpoint = useLightInk(background) ? 1.0 : 0.0;
  const double lightness = color.lightnessF();
  // AGENT-GUARD: Fit lightness while retaining hue. QST status foregrounds
  // belong on their paired badges, not arbitrary terminal backgrounds.
  for (int step = 1; step <= 256; ++step) {
    const QColor fitted = QColor::fromHslF(color.hslHueF(), color.hslSaturationF(),
        static_cast<float>(lightness + (endpoint - lightness) * step / 256.0));
    if (DesignTokenDeriver::contrastRatio(fitted, background) >= minimumContrast)
      return fitted;
  }
  // Some custom mid-tone backgrounds cannot reach 7:1 with any foreground.
  // Return the mathematically strongest opaque ink instead of false contrast.
  return endpoint == 1.0 ? QColor(Qt::white) : QColor(Qt::black);
}

std::array<QColor, 16> terminalAnsiPalette(const QColor &background,
                                        bool highContrast) {
  std::array<QColor, 16> result;
  const bool lightInk = useLightInk(background);
  const double contrast = highContrast ? 7.0 : 4.5;
  // AGENT-CONTRACT: ANSI indices are protocol hues, not status semantics.
  // Red, green, ochre/yellow, blue, orchid/magenta and cyan remain recognizable
  // across theme changes. See ADR-0101; explicit 24-bit output is untouched.
  constexpr int hues[] = {0, 4, 142, 43, 220, 304, 184, 0};
  for (int index = 0; index < 8; ++index) {
    const bool neutral = index == 0 || index == 7;
    for (int intense = 0; intense < 2; ++intense) {
      double lightness = lightInk ? (intense ? .81 : .66)
                                 : (intense ? .23 : .34);
      if (neutral)
        lightness = lightInk ? (index == 0 ? .59 : .84) + intense * .10
                            : (index == 0 ? .36 : .25) - intense * .10;
      const double saturation = neutral ? 0.0 : (intense ? .83 : .57);
      result[static_cast<std::size_t>(index + intense * 8)] = terminalReadableColor(
          QColor::fromHslF(static_cast<float>(hues[index] / 360.0),
                           static_cast<float>(saturation), static_cast<float>(lightness)),
          background, contrast);
    }
  }
  return result;
}
} // namespace QindaQt::Apps::Terminal
