// SPDX-License-Identifier: LGPL-3.0-or-later
#include "color_math_p.h"

#include "qindaqt/design_tokens/token_deriver.h"

#include <algorithm>
#include <cmath>

namespace QindaQt::DesignTokens::Private {
namespace {

float boundedChannel(double value)
{
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

double channel(const QColor &color, int index)
{
    switch (index) {
    case 0:
        return static_cast<double>(color.redF());
    case 1:
        return static_cast<double>(color.greenF());
    default:
        return static_cast<double>(color.blueF());
    }
}

} // namespace

QColor withAlpha(const QColor &color, double alpha)
{
    QColor result = color;
    result.setAlphaF(boundedChannel(alpha));
    return result;
}

QColor compositeOver(const QColor &foreground, const QColor &background)
{
    const double foregroundAlpha = static_cast<double>(foreground.alphaF());
    const double backgroundAlpha = static_cast<double>(background.alphaF());
    const double outputAlpha = foregroundAlpha + backgroundAlpha * (1.0 - foregroundAlpha);
    if (outputAlpha <= 0.0) {
        return QColor::fromRgbF(0.0F, 0.0F, 0.0F, 0.0F);
    }

    double channels[3] = {};
    for (int index = 0; index < 3; ++index) {
        const double premultiplied = channel(foreground, index) * foregroundAlpha
            + channel(background, index) * backgroundAlpha * (1.0 - foregroundAlpha);
        channels[index] = premultiplied / outputAlpha;
    }
    return QColor::fromRgbF(boundedChannel(channels[0]),
                            boundedChannel(channels[1]),
                            boundedChannel(channels[2]),
                            boundedChannel(outputAlpha));
}

QColor mix(const QColor &from, const QColor &toward, double amount)
{
    const double bounded = std::clamp(amount, 0.0, 1.0);
    return QColor::fromRgbF(
        boundedChannel(channel(from, 0) + (channel(toward, 0) - channel(from, 0)) * bounded),
        boundedChannel(channel(from, 1) + (channel(toward, 1) - channel(from, 1)) * bounded),
        boundedChannel(channel(from, 2) + (channel(toward, 2) - channel(from, 2)) * bounded),
        boundedChannel(static_cast<double>(from.alphaF())
                       + (static_cast<double>(toward.alphaF())
                          - static_cast<double>(from.alphaF()))
                           * bounded));
}

QColor contrastForeground(const QColor &background)
{
    const QColor black(Qt::black);
    const QColor white(Qt::white);
    return DesignTokenDeriver::contrastRatio(black, background)
            >= DesignTokenDeriver::contrastRatio(white, background)
        ? black
        : white;
}

} // namespace QindaQt::DesignTokens::Private
