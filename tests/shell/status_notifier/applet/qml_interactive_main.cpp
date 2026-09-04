// SPDX-License-Identifier: GPL-3.0-or-later

// Compiled QML harness for the StatusNotifier applet rows.
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
#include <QtGlobal>

#include "../../../controls/control_test_support.h"

class StatusNotifierAppletHarness final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready CONSTANT)

public:
    explicit StatusNotifierAppletHarness(QQmlEngine *engine)
    {
        if (engine == nullptr) {
            qFatal("status notifier applet harness singleton received no engine");
        }
        QString error;
        m_ready = QindaQt::Controls::TestSupport::publishTheme(
            *engine,
            QStringLiteral("qinda-light.json"),
            QindaQt::DesignTokens::AccessibilityInputs {},
            &error);
        if (!m_ready) {
            qFatal("status notifier applet QML harness could not publish tokens: %s",
                   qPrintable(error));
        }
    }

    [[nodiscard]] bool ready() const noexcept { return m_ready; }

private:
    bool m_ready = false;
};

class StatusNotifierAppletQmlSetup final : public QObject {
    Q_OBJECT

public:
    StatusNotifierAppletQmlSetup()
    {
        qmlRegisterSingletonType<StatusNotifierAppletHarness>(
            "QindaQt.Shell.StatusNotifier.Tests", 1, 0, "Harness",
            [](QQmlEngine *engine, QJSEngine *) {
                return new StatusNotifierAppletHarness(engine);
            });
    }

public slots:
    // AGENT-GUARD: themePath() fails closed without the pinned runtime theme
    // copies; pinDeterministicFonts() must run before any harness singleton
    // factory publishes qinda-light. (Clipboard harness precedent — without
    // this, the rows only pass after an unrelated Controls test happened to
    // write the pinned copies into the shared build directory first.)
    void applicationAvailable()
    {
        QindaQt::Controls::TestSupport::pinDeterministicFonts();
    }
};

QUICK_TEST_MAIN_WITH_SETUP(status_notifier_applet_interactive, StatusNotifierAppletQmlSetup)

#include "qml_interactive_main.moc"
