// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QGuiApplication;
class QQmlApplicationEngine;

namespace QindaQt::AppAppearance {
class ApplicationAppearanceController;
}
namespace QindaQt::DesignTokens {
class TokenFacade;
}

namespace QindaQt::Apps::Welcome {

[[nodiscard]] DesignTokens::TokenFacade *ensureWelcomeTokenFacade(
    QQmlApplicationEngine &engine, QString *error = nullptr);
[[nodiscard]] bool applyWelcomeAppearance(
    const AppAppearance::ApplicationAppearanceController &controller,
    DesignTokens::TokenFacade &facade, QGuiApplication &application,
    QString *error = nullptr);

} // namespace QindaQt::Apps::Welcome
