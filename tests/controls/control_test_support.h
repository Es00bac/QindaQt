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

// Resolves a theme file name against the pinned runtime copies written by
// pinDeterministicFonts() (never directly against data/themes/). Fails closed
// (qFatal) when the pinned copy does not exist, which means
// pinDeterministicFonts() has not run yet in this process.
[[nodiscard]] QString themePath(const QString &fileName);
[[nodiscard]] bool publishTheme(
    QQmlEngine &engine,
    const QString &fileName,
    const QindaQt::DesignTokens::AccessibilityInputs &inputs = {},
    QString *error = nullptr);
// Registers the byte-pinned visual fonts from fontDir (empty: the
// QINDAQT_CONTROLS_FONT_DIR build definition) into the process font database,
// rewrites every data/themes/*.json fontFamily/monoFontFamily onto the
// registered repository families in the pinned theme directory, and installs
// QFont substitutions for the original catalog names (substitutions only take
// effect when the requested family is absent from the host, which is why the
// theme copies are rewritten). Process-wide state: call once per process, on
// the GUI thread, before rendering any fixture text. Aborts the process
// (qFatal) when a fixture file is missing, unreadable, does not declare
// exactly the expected family names, or a theme catalog file cannot be read,
// parsed, or rewritten, so a broken fixture can never degrade into silent
// host-font rendering. The fontDir overload exists for the fail-closed
// negative-control tests.
void pinDeterministicFonts(const QString &fontDir = {});
[[nodiscard]] QColor objectColor(QObject *object);
[[nodiscard]] QObject *controlBackground(QObject *control);
[[nodiscard]] QAccessibleInterface *accessible(QObject *object);
[[nodiscard]] QQuickItem *item(QQuickItem *root, const char *name);
[[nodiscard]] QVariantMap completePreviewUsing(const QVariant &role);
void waitForMotion(QObject *control);

} // namespace QindaQt::Controls::TestSupport
