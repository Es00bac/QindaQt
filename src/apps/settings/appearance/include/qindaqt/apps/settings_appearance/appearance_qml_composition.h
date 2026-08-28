// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/token_facade.h"

#include <QQmlEngine>

class QString;

namespace QindaQt::Apps::SettingsAppearance {

// Route-composition seam for the qindaqt-settings application and focused
// tests. QST-1 requires one complete token publication before token-dependent
// controls are constructed, so callers must invoke this before loading the
// page QML and hand the returned facade to AppearanceSettingsModel.
[[nodiscard]] DesignTokens::TokenFacade *
ensureTokenFacade(QQmlEngine &engine, QString *error = nullptr);

} // namespace QindaQt::Apps::SettingsAppearance
