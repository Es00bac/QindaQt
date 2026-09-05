// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QtTest>

#include <memory>

namespace TaskListAppletQmlTest
{

// AGENT-NOTE: applet QML resolves QST-1 roles from the singleton in the same
// engine. This mirrors production publication before creating any applet.
inline bool publishTokens(QQmlEngine &engine, QString *error)
{
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    for (int spin = 0;
         spin < 100 && registration.status() == QQmlComponent::Loading; ++spin) {
        QTest::qWait(10);
    }
    if (!registration.isReady()) {
        *error = registration.errorString();
        return false;
    }
    std::unique_ptr<QObject> registrationObject(registration.create());
    if (registrationObject == nullptr) {
        *error = registration.errorString();
        return false;
    }
    auto *facade =
        engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
            "QindaQt.Tokens", "Tokens");
    if (facade == nullptr) {
        *error = QStringLiteral("QindaQt.Tokens singleton was not registered");
        return false;
    }
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!loaded.ok) {
        *error = loaded.error;
        return false;
    }
    return facade->publish(loaded.theme, {}, error);
}

} // namespace TaskListAppletQmlTest
