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

namespace QindaQt::Apps::RemovableMedia {

// GUI-thread only. Returns the engine-owned singleton, or nullptr with a
// diagnostic; callers must not retain it beyond the engine's lifetime.
[[nodiscard]] DesignTokens::TokenFacade *ensureMediaTokenFacade(
    QQmlApplicationEngine &engine, QString *error = nullptr);
// Borrows all arguments and publishes the same theme to QML, palette, and
// font. Returns false with a diagnostic when the theme cannot be derived.
[[nodiscard]] bool applyMediaAppearance(
    const AppAppearance::ApplicationAppearanceController &controller,
    DesignTokens::TokenFacade &facade, QGuiApplication &application,
    QString *error = nullptr);

} // namespace QindaQt::Apps::RemovableMedia
