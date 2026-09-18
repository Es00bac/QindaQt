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

namespace QindaQt::Apps::Osk {

// Registers the QindaQt.Tokens singleton in `engine` and returns the facade
// the keyboard's QML reads; the same bootstrap the other first-party apps use.
[[nodiscard]] DesignTokens::TokenFacade *ensureOskTokenFacade(QQmlEngine &engine, QString *error = nullptr);

// Publishes the controller's theme into the facade and the application font.
[[nodiscard]] bool applyOskAppearance(const AppAppearance::ApplicationAppearanceController &controller,
                                      DesignTokens::TokenFacade &facade, QGuiApplication &application,
                                      QString *error = nullptr);

} // namespace QindaQt::Apps::Osk
