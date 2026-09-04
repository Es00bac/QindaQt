// SPDX-License-Identifier: GPL-3.0-or-later
#include "shelltokenpublisher.h"

#include "qindaqt/design_tokens/accessibility_inputs.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_catalog.h"

#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTimer>

#include <memory>

namespace QindaQt::Shell {
namespace {

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
            ? QStringLiteral("timed out loading the shell QindaQt.Tokens facade")
            : component.errorString();
    }
    return false;
}

[[nodiscard]] DesignTokens::TokenFacade *ensureFacade(QQmlEngine &engine,
                                                       QString *error)
{
    // AGENT-NOTE: The singleton cannot be requested from C++ until an import
    // has registered its engine-owned instance. This happens before any panel
    // or hosted-applet QML is created.
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:shell-token-registration.qml")));
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
        *error = QStringLiteral("shell QindaQt.Tokens singleton was not registered");
    }
    return facade;
}

} // namespace

ShellTokenPublisher::ShellTokenPublisher(QQmlEngine &engine,
                                         Themes::ThemeCatalog &themes,
                                         QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_themes(themes)
{
}

bool ShellTokenPublisher::start(QString *error)
{
    if (m_facade != nullptr) {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    m_facade = ensureFacade(m_engine, error);
    if (m_facade == nullptr || !publishSelected(error)) {
        m_facade = nullptr;
        return false;
    }
    connect(&m_themes, &Themes::ThemeCatalog::currentChanged, this, [this] {
        QString publicationError;
        if (!publishSelected(&publicationError)) {
            emit publicationFailed(publicationError);
        }
    });
    return true;
}

DesignTokens::TokenFacade *ShellTokenPublisher::facade() const
{
    return m_facade;
}

bool ShellTokenPublisher::publishSelected(QString *error)
{
    const int index = m_themes.currentIndex();
    if (m_facade == nullptr || index < 0
        || static_cast<qsizetype>(index) >= m_themes.themes().size()) {
        if (error != nullptr) {
            *error = QStringLiteral("shell token publication has no selected theme");
        }
        return false;
    }
    // AGENT-CONTRACT: Every selected theme reaches QindaQt.Tokens as one
    // complete generation before Controls consumers observe currentChanged.
    return m_facade->publish(m_themes.themes().at(index),
                             DesignTokens::AccessibilityInputs{}, error);
}

} // namespace QindaQt::Shell
