// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QIcon>
#include <QString>

namespace QindaQt::Controls {
// GUI-thread value lookup. Uses Qt's selected public icon theme with the
// packaged QindaQt catalog as fallback. Names are bounded freedesktop names,
// never paths; invalid or unknown names return a null icon. The returned value
// owns its image engine and may outlive any control. No shell runtime dependency.
[[nodiscard]] QIcon applicationIcon(const QString &name);
}
