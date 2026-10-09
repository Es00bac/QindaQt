// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/managed_applications.h"

namespace ManagedCatalogFixtures {
using namespace QindaQt::ShellLauncher;

// Synthetic catalog values only; these fixtures are never authenticated
// recipes/materialization receipts and never start an Android/Wine runtime.
inline ManagedApplicationRegistration registration(
    ApplicationOrigin origin = ApplicationOrigin::Android)
{
    ManagedApplicationRegistration value;
    value.binding = {origin, QStringLiteral("12345678-1234-4234-8234-123456789abc"),
                     1, QString(64, QLatin1Char('a'))};
    value.name = QStringLiteral("Fixture Calculator");
    value.iconName = QStringLiteral("accessories-calculator");
    value.categories = {QStringLiteral("Utility"), QStringLiteral("Calculator")};
    value.keywords = {QStringLiteral("arithmetic")};
    return value;
}

inline QString desktop(const QString &extra = {})
{
    return QStringLiteral("[Desktop Entry]\nType=Application\nName=Native Fixture\n"
                          "Icon=applications-utilities\nExec=/bin/true\n") + extra;
}
} // namespace ManagedCatalogFixtures
