// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "qindaqt/decoration_painter/decoration_painter.h"
#include <QImage>

namespace QindaQt::Decoration {
// Owned immutable value in logical pixels. Empty image means no shadow.
// The eight cells surrounding innerRect are the KDecoration nine-patch;
// padding positions them around the actual frame. Pure, thread-independent,
// no KDecoration/KWin dependency or retained painter/filesystem state.
struct DecorationShadowTexture {
    QImage image;
    QMarginsF padding;
    QRectF innerRect;
};
[[nodiscard]] DecorationShadowTexture decorationShadowTexture(const DecorationVisualStyle &style);
[[nodiscard]] DecorationShadowTexture decorationShadowTexture(const DecorationVisualStyle &style,
    const DecorationChrome &chrome, const DecorationFrameVisual &frame);
// Draws the same eight texture cells used by the live decoration, at origin.
// Caller positions the frame; the helper preserves painter state.
void paintDecorationShadow(QPainter &painter, const DecorationVisualStyle &style,
                           const DecorationChrome &chrome, const DecorationFrameVisual &frame);
}
