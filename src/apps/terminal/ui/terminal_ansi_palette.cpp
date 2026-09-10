// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_ansi_palette.h"

#include <algorithm>
#include <cmath>

namespace QindaQt::Apps::Terminal {
namespace {

double linearizedChannel(double channel) {
  // WCAG 2.x sRGB linearization.
  return channel <= 0.04045 ? channel / 12.92
                            : std::pow((channel + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const QColor &color) {
  return 0.2126 * linearizedChannel(color.redF()) +
         0.7152 * linearizedChannel(color.greenF()) +
         0.0722 * linearizedChannel(color.blueF());
}

bool useLightInk(const QColor &background) {
  return terminalContrastRatio(Qt::white, background) >=
         terminalContrastRatio(Qt::black, background);
}
} // namespace

double terminalContrastRatio(const QColor &first, const QColor &second) {
  const double lighter = std::max(relativeLuminance(first), relativeLuminance(second));
  const double darker = std::min(relativeLuminance(first), relativeLuminance(second));
  return (lighter + 0.05) / (darker + 0.05);
}

QColor terminalReadableColor(QColor color, const QColor &background,
                             double minimumContrast) {
  color.setAlpha(255);
  if (terminalContrastRatio(color, background) >= minimumContrast)
    return color;
  const double endpoint = useLightInk(background) ? 1.0 : 0.0;
  const double lightness = color.lightnessF();
  // AGENT-GUARD: Fit lightness while retaining hue. Protocol and status
  // foregrounds must stay recognizable on the terminal background.
  for (int step = 1; step <= 256; ++step) {
    const QColor fitted = QColor::fromHslF(color.hslHueF(), color.hslSaturationF(),
        static_cast<float>(lightness + (endpoint - lightness) * step / 256.0));
    if (terminalContrastRatio(fitted, background) >= minimumContrast)
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
  // across scheme changes. See ADR-0112; explicit 24-bit output is untouched.
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
