// SPDX-License-Identifier: GPL-3.0-or-later

// Compiled QML harness for the Clipboard applet rows.
//
// AGENT-GUARD: the applet's QML consumes QST-1 token values (spacing, radii,
// type scale) that exist only after a theme is published to the Tokens
// singleton. A bare qmltestrunner run evaluates those bindings against an
// empty singleton: geometry collapses to zero, real pointer events hit
// nothing, and undefined-to-double warnings flood the output. This harness
// publishes a real theme before any test executes, so layout, real-event
// delivery, and warning-free runs are all genuine evidence.

#include <QtQuickTest/quicktest.h>

#include <QQmlEngine>
#include <QString>
#include <QtGlobal>

#include "../../controls/control_test_support.h"

class ClipboardAppletQmlSetup final : public QObject {
    Q_OBJECT

public:
    ClipboardAppletQmlSetup() = default;

public Q_SLOTS:
    void qmlEngineCreated(QQmlEngine *engine)
    {
        if (engine == nullptr) {
            qFatal("clipboard applet QML harness received no engine");
        }
        QString error;
        const bool published = QindaQt::Controls::TestSupport::publishTheme(
            *engine,
            QStringLiteral("qinda-light.json"),
            QindaQt::DesignTokens::AccessibilityInputs {},
            &error);
        if (!published) {
            qFatal("clipboard applet QML harness could not publish tokens: %s",
                   qPrintable(error));
        }
    }
};

QUICK_TEST_MAIN_WITH_SETUP(clipboard_applet_interactive, ClipboardAppletQmlSetup)

#include "qml_interactive_main.moc"
