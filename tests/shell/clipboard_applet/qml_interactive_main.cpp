// SPDX-License-Identifier: GPL-3.0-or-later

// Compiled QML harness for the Clipboard applet rows.
//
// AGENT-GUARD: the applet's QML consumes QST-1 token values (spacing, radii,
// type scale) that exist only after a theme is published to the Tokens
// singleton of the SAME engine that runs the test. Qt 6.11 QuickTest invokes
// no engine-created setup hook (only applicationAvailable, before any engine
// exists), so publication happens in the factory of the registered Harness
// singleton: its factory receives the live test engine, publishes the real
// qinda-light theme through the public Controls test-support path, and every
// test file forces instantiation by reading `Harness.ready` in init() before
// any applet component is created. A bare qmltestrunner run cannot provide
// this; do not convert these rows back to it.

#include <QtQuickTest/quicktest.h>

#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QtGlobal>

#include "../../controls/control_test_support.h"

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

class ClipboardAppletHarness final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready CONSTANT)

public:
    explicit ClipboardAppletHarness(QQmlEngine *engine)
    {
        if (engine == nullptr) {
            qFatal("clipboard applet harness singleton received no engine");
        }
        QString error;
        m_ready = QindaQt::Controls::TestSupport::publishTheme(
            *engine,
            QStringLiteral("qinda-light.json"),
            QindaQt::DesignTokens::AccessibilityInputs {},
            &error);
        if (!m_ready) {
            qFatal("clipboard applet QML harness could not publish tokens: %s",
                   qPrintable(error));
        }
    }

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
    bool m_ready = false;
};

class ClipboardAppletQmlSetup final : public QObject {
    Q_OBJECT

public:
    ClipboardAppletQmlSetup()
    {
        qmlRegisterSingletonType<ClipboardAppletHarness>(
            "QindaQt.Shell.ClipboardApplet.Tests", 1, 0, "Harness",
            [](QQmlEngine *engine, QJSEngine *) {
                return new ClipboardAppletHarness(engine);
            });
    }

public slots:
    void applicationAvailable()
    {
        QindaQt::Controls::TestSupport::pinDeterministicFonts();
    }
};

QUICK_TEST_MAIN_WITH_SETUP(clipboard_applet_interactive, ClipboardAppletQmlSetup)

#include "qml_interactive_main.moc"
