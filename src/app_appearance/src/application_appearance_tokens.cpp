// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/app_appearance/application_appearance_controller.h>

#include <qindaqt/design_tokens/token_facade.h>

namespace QindaQt::AppAppearance {

bool ApplicationAppearanceController::publishTokens(
    DesignTokens::TokenFacade &facade, QString *error) const {
  if (m_theme.id.isEmpty()) {
    if (error)
      *error = QStringLiteral("No validated application theme");
    return false;
  }
  return facade.publish(m_theme, accessibilityInputs(), error);
}

} // namespace QindaQt::AppAppearance
