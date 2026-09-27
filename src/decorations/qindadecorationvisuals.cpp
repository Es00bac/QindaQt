// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindadecorationvisuals.h"
#include "qindaqt/decoration_painter/decoration_shadow.h"
#include <KDecoration3/DecorationShadow>

namespace QindaQt::Decoration {
namespace {
std::shared_ptr<KDecoration3::DecorationShadow> nativeShadow(const DecorationShadowTexture &texture)
{
    if (texture.image.isNull()) return {};
    auto shadow = std::make_shared<KDecoration3::DecorationShadow>();
    shadow->setPadding(texture.padding);
    shadow->setInnerShadowRect(texture.innerRect);
    shadow->setShadow(texture.image);
    return shadow;
}
}

std::shared_ptr<KDecoration3::DecorationShadow>
createDecorationShadow(const DecorationVisualStyle &style)
{
    return nativeShadow(decorationShadowTexture(style));
}

std::shared_ptr<KDecoration3::DecorationShadow>
createDecorationShadow(const DecorationVisualStyle &style,
                       const DecorationChrome &chrome, const DecorationFrameVisual &frame)
{
    return nativeShadow(decorationShadowTexture(style, chrome, frame));
}
}
