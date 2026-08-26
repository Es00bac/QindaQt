// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/settings_schema.h"

namespace QindaQt::Settings::Internal {

// AGENT-NOTE: This is private schema machinery, split from JSON parsing so type
// coercion and constraint semantics have one focused implementation and test path.
[[nodiscard]] bool normalizeSettingValue(const SettingDefinition &definition,
                                         const QVariant &input,
                                         QVariant *normalized,
                                         QString *message);

} // namespace QindaQt::Settings::Internal
