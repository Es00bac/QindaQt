// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"

#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTimer>

#include <memory>

namespace QindaQt::Apps::SettingsAppearance {
namespace {

// AGENT-NOTE: An import-bearing inline component can still be Loading when
// create() is first called; spinning the event loop until a terminal status
// mirrors the qualified Controls test-support approach. The deadline keeps a
// missing module an honest error instead of a hang.
[[nodiscard]] bool awaitComponent(QQmlComponent &component, QString *error)
{
    if (component.status() == QQmlComponent::Loading) {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&component, &QQmlComponent::statusChanged, &loop,
                         [&loop](QQmlComponent::Status status) {
                             if (status != QQmlComponent::Loading) {
                                 loop.quit();
                             }
                         });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(5'000);
        loop.exec();
    }
    if (component.isReady()) {
        return true;
    }
    if (error != nullptr) {
        *error = component.status() == QQmlComponent::Loading
            ? QStringLiteral("timed out loading the QindaQt.Tokens component")
            : component.errorString();
    }
    return false;
}

} // namespace

DesignTokens::TokenFacade *
ensureTokenFacade(QQmlEngine &engine, QString *error)
{
    // AGENT-NOTE: Importing once through a throwaway component instantiates
    // the engine-owned QindaQt.Tokens singleton through the module plugin, so
    // composition can publish tokens before page QML exists.
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:qst-token-registration.qml")));
    if (!awaitComponent(registration, error)) {
        return nullptr;
    }
    std::unique_ptr<QObject> registrationObject(registration.create());
    if (!registrationObject) {
        if (error != nullptr) {
            *error = registration.errorString();
        }
        return nullptr;
    }
    auto *facade = engine.singletonInstance<DesignTokens::TokenFacade *>(
        QStringLiteral("QindaQt.Tokens"), QStringLiteral("Tokens"));
    if (facade == nullptr && error != nullptr) {
        *error = QStringLiteral("QindaQt.Tokens singleton was not registered");
    }
    return facade;
}

} // namespace QindaQt::Apps::SettingsAppearance
