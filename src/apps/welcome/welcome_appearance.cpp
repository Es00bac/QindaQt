// SPDX-License-Identifier: GPL-3.0-or-later
#include "welcome_appearance.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/design_tokens/token_facade.h"

#include <QEventLoop>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QTimer>

#include <memory>

namespace QindaQt::Apps::Welcome {

DesignTokens::TokenFacade *ensureWelcomeTokenFacade(QQmlApplicationEngine &engine,
                                                     QString *error) {
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml", QUrl(QStringLiteral("inline:qindaqt-welcome-token-registration.qml")));
    if (registration.status() == QQmlComponent::Loading) {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&registration, &QQmlComponent::statusChanged, &loop,
                         [&loop](QQmlComponent::Status status) {
                             if (status != QQmlComponent::Loading) {
                                 loop.quit();
                             }
                         });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(5'000);
        loop.exec();
    }
    if (!registration.isReady()) {
        if (error != nullptr) {
            *error = registration.errorString().trimmed();
        }
        return nullptr;
    }
    std::unique_ptr<QObject> registrationObject(registration.create());
    if (!registrationObject) {
        if (error != nullptr) {
            *error = registration.errorString().trimmed();
        }
        return nullptr;
    }
    auto *facade = engine.singletonInstance<DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (facade == nullptr && error != nullptr) {
        *error = QStringLiteral("QindaQt.Tokens singleton was not registered");
    }
    return facade;
}

bool applyWelcomeAppearance(
    const AppAppearance::ApplicationAppearanceController &controller,
    DesignTokens::TokenFacade &facade, QGuiApplication &application,
    QString *error) {
    if (!controller.publishTokens(facade, error)) {
        return false;
    }
    const auto derived = DesignTokens::DesignTokenDeriver::derive(controller.theme(), {});
    if (!derived.ok()) {
        if (error != nullptr) {
            *error = derived.diagnostic;
        }
        return false;
    }
    const auto &tokens = *derived.tokens;
    QPalette palette;
    palette.setColor(QPalette::Window, tokens.background().base);
    palette.setColor(QPalette::WindowText, tokens.foreground().defaultColor);
    palette.setColor(QPalette::Base, tokens.background().raised);
    palette.setColor(QPalette::AlternateBase, tokens.background().highest);
    palette.setColor(QPalette::Text, tokens.foreground().defaultColor);
    palette.setColor(QPalette::Button, tokens.background().raised);
    palette.setColor(QPalette::ButtonText, tokens.foreground().defaultColor);
    palette.setColor(QPalette::Highlight, tokens.accent().defaultColor);
    palette.setColor(QPalette::HighlightedText, tokens.accent().foreground);
    palette.setColor(QPalette::PlaceholderText, tokens.foreground().muted);
    palette.setColor(QPalette::Disabled, QPalette::Text, tokens.foreground().disabled);
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     tokens.foreground().disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     tokens.foreground().disabled);
    application.setPalette(palette);
    QFont font(tokens.typeScale().fontFamily);
    font.setPointSizeF(tokens.typeScale().body);
    application.setFont(font);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace QindaQt::Apps::Welcome
