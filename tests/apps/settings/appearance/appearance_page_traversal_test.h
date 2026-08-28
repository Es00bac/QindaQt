// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QQuickItem;
class QQuickView;
class QString;

[[nodiscard]] bool verifyFullAppearanceTraversal(QQuickView &view,
                                                 QQuickItem *root,
                                                 QString *error);
