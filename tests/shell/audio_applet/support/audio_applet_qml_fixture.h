// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Common compiled-applet setup for keyboard, drag, focus, and wheel QML rows.
// Keep the icon/theme/runtime fixture in one place so the focused suites
// exercise the same production import path and popup window.
#include "audio_applet_controller.h"
#include "../../icon_resolution_test_fixture.h"
#include "support/fake_audio_transport.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QPointer>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

using namespace QindaQt;
using namespace QindaQt::Shell::AudioApplet;
using namespace QindaQt::Tests;

namespace {

const QString kOwner = QStringLiteral(":1.42");

QList<QQuickItem *> visualItemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root == nullptr)
        return matches;
    if (root->objectName() == name)
        matches.append(root);
    for (QQuickItem *child : root->childItems())
        matches.append(visualItemsNamed(child, name));
    return matches;
}

int countPendingRows(const AudioAppletController &controller)
{
    int pending = 0;
    const QVariantList devices = controller.deviceRows();
    for (const QVariant &value : devices) {
        if (value.value<DeviceRow>().pending()) {
            ++pending;
        }
    }
    const QVariantList streams = controller.streamRows();
    for (const QVariant &value : streams) {
        if (value.value<StreamRow>().pending()) {
            ++pending;
        }
    }
    return pending;
}

[[nodiscard]] inline DeviceRow firstDeviceRow(const AudioAppletController &controller)
{
    const QVariantList rows = controller.deviceRows();
    return rows.isEmpty() ? DeviceRow{} : rows.constFirst().value<DeviceRow>();
}

// The compiled applet needs an import path, resolved icons, and a published
// theme before it can be instantiated, and every row in this file needs
// exactly that.
struct AppletHarness {
    QQmlEngine engine;
    std::unique_ptr<QObject> tokenRegistration;
    std::unique_ptr<QObject> applet;

    [[nodiscard]] QQuickItem *root() const
    {
        return qobject_cast<QQuickItem *>(applet.get());
    }
};

[[nodiscard]] bool loadApplet(AppletHarness &harness,
                              AudioAppletController *controller,
                              const QStringList &iconNames, QString *error)
{
    harness.engine.addImportPath(
        QStringLiteral(QINDAQT_AUDIO_APPLET_QML_IMPORT_PATH));
    if (!Tests::installResolvedIconFixture(
            harness.engine, QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
            iconNames, error)) {
        return false;
    }

    // AGENT-NOTE: QindaQt.Controls resolves QST-1 roles from the read-only
    // Tokens singleton; without a published theme the state cards render
    // undefined tokens. Publication is the same seam production composition
    // uses, exercised here through the generated plugin path.
    QQmlComponent registration(&harness.engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    if (!QTest::qWaitFor([&registration] {
            return registration.status() != QQmlComponent::Loading;
        })) {
        *error = QStringLiteral("timed out loading the QindaQt.Tokens module");
        return false;
    }
    if (!registration.isReady()) {
        *error = registration.errorString();
        return false;
    }
    harness.tokenRegistration.reset(registration.create());
    if (harness.tokenRegistration == nullptr) {
        *error = QStringLiteral("the QindaQt.Tokens registration failed");
        return false;
    }

    auto *facade = harness.engine.singletonInstance<DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (facade == nullptr) {
        *error = QStringLiteral("the Tokens singleton did not resolve");
        return false;
    }
    const auto loaded = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!loaded.ok) {
        *error = loaded.error;
        return false;
    }
    if (!facade->publish(loaded.theme, {}, error)) {
        return false;
    }

    QQmlComponent component(&harness.engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.AudioApplet"),
                             QStringLiteral("AudioApplet"));
    if (!component.isReady()) {
        *error = component.errorString();
        return false;
    }
    harness.applet.reset(component.createWithInitialProperties(
        {{QStringLiteral("controller"), QVariant::fromValue(controller)}}));
    if (harness.applet == nullptr) {
        *error = component.errorString();
        return false;
    }
    return true;
}

// Returns the opened details popup's content item; all device and stream
// controls live inside it.
[[nodiscard]] QQuickItem *openPopupContent(QQuickItem *root,
                                           QQuickWindow *window)
{
    auto *popup = root->findChild<QObject *>(
        QStringLiteral("audioAppletPopup"));
    if (popup == nullptr)
        return nullptr;
    QTest::keyClick(window, Qt::Key_Return);
    if (!QTest::qWaitFor([popup] {
            return popup->property("opened").toBool();
        })) {
        return nullptr;
    }
    return popup->property("contentItem").value<QQuickItem *>();
}

} // namespace
