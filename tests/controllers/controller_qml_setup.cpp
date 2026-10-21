// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_input/controllers_model.h"
#include "qindaqt/controllers/controller_policy.h"
#include <qindaqt/design_tokens/token_facade.h>
#include <qindaqt/design_tokens/accessibility_inputs.h>
#include <qindaqt/themes/theme_loader.h>
#include <QtQuickTest/quicktest.h>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTimer>
using namespace QindaQt;
class EditorPort final : public Apps::SettingsInput::ControllerPort {
    Q_OBJECT
public:
    Controllers::Profile profile = Controllers::defaultProfile("dualsense");
    quint64 revision = 1;
    void refresh() override {
        Q_EMIT snapshotReceived({{"schemaVersion", 2}, {"revision", QString::number(revision)},
            {"controllers", QJsonArray{QJsonObject{{"id", "default:dualsense"}, {"name", "DualSense defaults"},
                {"family", "dualsense"}, {"template", true}, {"config", Controllers::profileJson(profile)}}}}});
    }
    void apply(const QString &, const QJsonObject &patch, quint64 expected) override {
        QString reason;
        const bool ok = expected == revision && Controllers::applyPatch(patch, profile, reason);
        if (ok) ++revision;
        QTimer::singleShot(0, this, [this, ok, reason] { refresh(); Q_EMIT completed(ok, reason); });
    }
    void reset(const QString &, quint64) override { profile = Controllers::defaultProfile("dualsense"); ++revision; refresh(); Q_EMIT completed(true, "ok"); }
};
class ControllerEditorSetup final : public QObject {
    Q_OBJECT
    EditorPort port;
    Apps::SettingsInput::ControllersModel model{port};
public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine) {
        auto *tokens = engine->singletonInstance<DesignTokens::TokenFacade *>("QindaQt.Tokens", "Tokens");
        const auto theme = Themes::ThemeLoader::fromFile("/usr/share/qindaqt/themes/qinda-dark.json");
        QString error;
        if (!tokens || !theme.ok || !tokens->publish(theme.theme, {}, &error)) qFatal("Cannot initialize installed QST theme for controller editor fixture");
        engine->rootContext()->setContextProperty("testControllers", &model);
        port.refresh();
    }
};
QUICK_TEST_MAIN_WITH_SETUP(controller_editor, ControllerEditorSetup)
#include "controller_qml_setup.moc"
