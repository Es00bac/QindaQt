// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
class QObject;
namespace QindaQt::Apps::FileManager {
class NavigationController;
bool verifySelectionUi(QObject *root, NavigationController *navigation,
                       const QString &fixtureRoot, QString *error);
}
