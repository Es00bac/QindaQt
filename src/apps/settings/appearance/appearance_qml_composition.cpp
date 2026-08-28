// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"

#include <QQmlComponent>
#include <QQmlEngine>

#include <memory>

namespace QindaQt::Apps::SettingsAppearance {

DesignTokens::TokenFacade *
ensureTokenFacade(QQmlEngine &engine, QString *error)
{
    // AGENT-NOTE: Importing once through a throwaway component instantiates
    // the engine-owned QindaQt.Tokens singleton through the module plugin, so
    // composition can publish tokens before page QML exists. This mirrors the
    // qualified Controls test-support approach instead of relying on static
    // registration side effects.
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:qst-token-registration.qml")));
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
