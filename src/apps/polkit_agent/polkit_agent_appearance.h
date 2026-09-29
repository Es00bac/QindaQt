// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QGuiApplication;
class QQmlEngine;

namespace QindaQt::AppAppearance {
class ApplicationAppearanceController;
}
namespace QindaQt::DesignTokens {
class TokenFacade;
}

namespace QindaQt::Apps::PolkitAgent {

// Registers the QindaQt.Tokens singleton in `engine` and returns the facade
// the dialog's QML reads; the same bootstrap every first-party app uses
// (compare src/apps/osk/osk_appearance.h).
[[nodiscard]] DesignTokens::TokenFacade *ensurePolkitAgentTokenFacade(QQmlEngine &engine,
                                                                      QString *error = nullptr);

// Publishes the controller's theme into the facade and the application font,
// so the authentication dialog follows the session's theme, including dark
// mode, exactly like every other first-party window.
[[nodiscard]] bool applyPolkitAgentAppearance(
    const AppAppearance::ApplicationAppearanceController &controller,
    DesignTokens::TokenFacade &facade, QGuiApplication &application, QString *error = nullptr);

} // namespace QindaQt::Apps::PolkitAgent
