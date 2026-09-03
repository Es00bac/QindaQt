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
// Registers the byte-pinned visual fonts from fontDir (empty: the
// QINDAQT_CONTROLS_FONT_DIR build definition) into the process font database
// and substitutes the theme schema families onto them. Process-wide state:
// call once per process, on the GUI thread, before rendering any fixture
// text. Aborts the process (qFatal) when a fixture file is missing,
// unreadable, or does not declare exactly the expected family names, so a
// broken fixture can never degrade into silent host-font rendering. The
// fontDir overload exists for the fail-closed negative-control tests.
void pinDeterministicFonts(const QString &fontDir = {});
[[nodiscard]] QColor objectColor(QObject *object);
[[nodiscard]] QObject *controlBackground(QObject *control);
[[nodiscard]] QAccessibleInterface *accessible(QObject *object);
[[nodiscard]] QQuickItem *item(QQuickItem *root, const char *name);
[[nodiscard]] QVariantMap completePreviewUsing(const QVariant &role);
void waitForMotion(QObject *control);

} // namespace QindaQt::Controls::TestSupport
