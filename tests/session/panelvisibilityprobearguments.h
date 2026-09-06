// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace QindaQt::Test::PanelVisibilityProbe {

struct Arguments final {
    QString captureTool;
    QString captureLibraryPath;
    QString controlFile;
    QString title = QStringLiteral("QindaQt panel visibility proof client");
};

[[nodiscard]] bool parseArguments(const QStringList &arguments, Arguments *result,
                                  QString *error);

} // namespace QindaQt::Test::PanelVisibilityProbe
