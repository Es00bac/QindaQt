// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCoreApplication>

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) {
        return 2;
    }

    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(application.arguments().at(1));
    if (!theme.ok) {
        return 3;
    }
    const auto result = QindaQt::DesignTokens::DesignTokenDeriver::derive(
        theme.theme,
        {.basePointSize = 11.0,
         .textScale = 1.25,
         .reducedMotion = true,
         .reducedTransparency = true,
         .highContrast = false});
    if (!result.ok()) {
        return 4;
    }
    return result.tokens->toVariantMap().size() == 16 ? 0 : 5;
}
