// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

namespace QindaQt::Core {
class WindowContainer;
}

namespace QindaQt::Compositor {

[[nodiscard]] bool applyOperation(Core::WindowContainer &container,
                                  const QJsonObject &operation,
                                  QString *code,
                                  QString *error);

} // namespace QindaQt::Compositor
