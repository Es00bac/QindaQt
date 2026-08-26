// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/hybrid_chrome/chrometypes.h"

namespace QindaQt::HybridChrome::TestFixtures {

inline ChromeLayoutRequest baseRequest()
{
    ChromeLayoutRequest request;
    request.containerId = QStringLiteral("container-alpha");
    request.outerRect = QRectF(0.0, 0.0, 1000.0, 700.0);
    request.style = ChromeStyle::standard(ButtonSide::Right);
    request.tabs = {{QStringLiteral("page-a"), QStringLiteral("Alpha"), true},
                    {QStringLiteral("page-b"), QStringLiteral("Beta"), false},
                    {QStringLiteral("page-c"), QStringLiteral("Gamma"), false}};
    // With default metrics, content is exactly (1, 69) through (999, 699).
    request.members = {{QStringLiteral("member-a"), QStringLiteral("Editor"),
                        QRectF(1.0, 69.0, 499.0, 630.0)},
                       {QStringLiteral("member-b"), QStringLiteral("Terminal"),
                        QRectF(500.0, 69.0, 499.0, 630.0)}};
    request.dividers = {{QStringLiteral("divider-main"), DividerOrientation::Vertical,
                         500.0, 69.0, 699.0}};
    return request;
}

inline ChromeLayoutRequest qindaMacRequest()
{
    auto request = baseRequest();
    request.style = ChromeStyle::qindaMacOS(request.style.palette);
    return request;
}

} // namespace QindaQt::HybridChrome::TestFixtures
