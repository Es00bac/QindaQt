// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Offscreen QML harness for the compiled QindaQt.Shell.DesktopControls
// module: publishes QST-1 from the shipped dark theme (the shell does this
// before any panel QML exists), creates one applet component with an injected
// facade, and hosts it in a small QQuickWindow. Runs under QT_FATAL_WARNINGS.

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>

#include <memory>

namespace QindaQt::Tests::DesktopControls {

inline bool publishTokens(QQmlEngine &engine)
{
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml", QUrl(QStringLiteral("inline:desktop-controls-token-registration.qml")));
    if (registration.status() == QQmlComponent::Loading) {
        QEventLoop loop;
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        QObject::connect(&registration, &QQmlComponent::statusChanged, &loop,
                         [&loop](QQmlComponent::Status status) {
                             if (status != QQmlComponent::Loading)
                                 loop.quit();
                         });
        loop.exec();
    }
    if (!registration.isReady())
        return false;
    std::unique_ptr<QObject> registrationObject(registration.create());
    auto *facade = engine.singletonInstance<DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    const auto loaded = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QString error;
    return registrationObject != nullptr && facade != nullptr && loaded.ok
        && facade->publish(loaded.theme, {}, &error);
}

struct AppletHost {
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QObject> root;
    std::unique_ptr<QQuickWindow> window;
    QQuickItem *item = nullptr;

    // Creates `typeName` from the compiled module with `access` injected and
    // shows it in a 640x480 offscreen window at (20, 20).
    bool create(const QString &typeName, QObject *access, QString *error, bool vertical = false)
    {
        engine = std::make_unique<QQmlEngine>();
        engine->addImportPath(QStringLiteral(QINDAQT_DESKTOP_CONTROLS_QML_IMPORT_PATH));
        if (!publishTokens(*engine)) {
            *error = QStringLiteral("token publication failed");
            return false;
        }
        QQmlComponent component(engine.get());
        component.loadFromModule(QStringLiteral("QindaQt.Shell.DesktopControls"), typeName);
        if (!component.isReady()) {
            *error = component.errorString();
            return false;
        }
        root.reset(component.createWithInitialProperties(
            {{QStringLiteral("access"), QVariant::fromValue(access)},
             {QStringLiteral("vertical"), vertical}}));
        if (!root) {
            *error = component.errorString();
            return false;
        }
        item = qobject_cast<QQuickItem *>(root.get());
        if (item == nullptr) {
            *error = QStringLiteral("component root is not an item");
            return false;
        }
        window = std::make_unique<QQuickWindow>();
        window->setGeometry(0, 0, 640, 480);
        item->setParentItem(window->contentItem());
        item->setPosition(QPointF(20, 20));
        item->setWidth(item->implicitWidth());
        item->setHeight(28);
        window->show();
        return true;
    }

    template <typename T>
    T *child(const QString &objectName) const
    {
        return root->findChild<T *>(objectName);
    }

    void focus(QQuickItem *target)
    {
        if (window != nullptr) {
            window->requestActivate();
            QTest::qWait(0);
        }
        target->forceActiveFocus(Qt::TabFocusReason);
    }

    // Popup windows own their own QQuickWindow; visual children of a Popup
    // therefore live under the popup's contentItem, which findChild reaches
    // because Popup's QObject tree still parents them to the popup.
    QList<QQuickItem *> visualItemsNamed(const QString &name) const
    {
        QList<QQuickItem *> matches;
        const auto visit = [&matches, &name](auto &&self, QQuickItem *current) -> void {
            if (current->objectName() == name && !matches.contains(current))
                matches.append(current);
            for (QQuickItem *child : current->childItems())
                self(self, child);
        };
        if (item != nullptr)
            visit(visit, item);
        for (QWindow *candidateWindow : QGuiApplication::allWindows()) {
            auto *quickWindow = qobject_cast<QQuickWindow *>(candidateWindow);
            if (quickWindow != nullptr)
                visit(visit, quickWindow->contentItem());
        }
        for (QQuickItem *candidate : root->findChildren<QQuickItem *>(name)) {
            if (!matches.contains(candidate))
                matches.append(candidate);
        }
        return matches;
    }
};

// Sends a key to whichever window currently holds focus (a popup window once
// one is open), mirroring how a user types into the separate popup surface.
inline void keyClickFocused(AppletHost &host, Qt::Key key, Qt::KeyboardModifiers modifiers = {})
{
    QWindow *target = QGuiApplication::focusWindow();
    if (target == nullptr) {
        target = host.window.get();
    }
    QTest::keyClick(target, key, modifiers);
}

inline void keyClicksFocused(AppletHost &host, const QString &text)
{
    QWindow *target = QGuiApplication::focusWindow();
    if (target == nullptr) {
        target = host.window.get();
    }
    // Qt 6.11 removed keyClicks(QWindow*, QString); search tests use ASCII.
    for (const QChar character : text) {
        if (character >= QLatin1Char('a') && character <= QLatin1Char('z')) {
            const auto key = static_cast<Qt::Key>(Qt::Key_A
                                                  + character.toUpper().unicode()
                                                  - QLatin1Char('A').unicode());
            QTest::keyClick(target, key);
        } else if (character >= QLatin1Char('A') && character <= QLatin1Char('Z')) {
            const auto key = static_cast<Qt::Key>(Qt::Key_A
                                                  + character.unicode()
                                                  - QLatin1Char('A').unicode());
            QTest::keyClick(target, key, Qt::ShiftModifier);
        } else {
            QTest::keyClick(target, Qt::Key_unknown);
        }
    }
}

// QQuickPopup::PopupType::Window; the enum is private API, so the literal is
// pinned here once with its name.
inline constexpr int PopupTypeWindow = 1;

} // namespace QindaQt::Tests::DesktopControls
