// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/accessibility_inputs.h"

#include <QColor>
#include <QString>
#include <QVariantMap>

class QQmlEngine;
class QObject;
class QAccessibleInterface;
class QQuickItem;

namespace QindaQt::Controls::TestSupport {

[[nodiscard]] QString themePath(const QString &fileName);
[[nodiscard]] bool publishTheme(
    QQmlEngine &engine,
    const QString &fileName,
    const QindaQt::DesignTokens::AccessibilityInputs &inputs = {},
    QString *error = nullptr);
void pinDeterministicFonts();
[[nodiscard]] QColor objectColor(QObject *object);
[[nodiscard]] QObject *controlBackground(QObject *control);
[[nodiscard]] QAccessibleInterface *accessible(QObject *object);
[[nodiscard]] QQuickItem *item(QQuickItem *root, const char *name);
[[nodiscard]] QVariantMap completePreviewUsing(const QVariant &role);
void waitForMotion(QObject *control);

} // namespace QindaQt::Controls::TestSupport
