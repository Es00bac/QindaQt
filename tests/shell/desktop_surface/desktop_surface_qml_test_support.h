// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Offscreen harness for the compiled QindaQt.Shell.DesktopSurface module:
// publishes QST-1 from the shipped dark theme (the shell does this before any
// panel QML exists), creates the DesktopSurface window over stub facades,
// and hosts it offscreen. Runs under QT_FATAL_WARNINGS.

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QEventLoop>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QVariantMap>
#include <QtTest>

#include <memory>

namespace QindaQt::Tests::DesktopSurface {

inline bool publishTokens(QQmlEngine &engine)
{
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml", QUrl(QStringLiteral("inline:desktop-surface-token-registration.qml")));
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
    if (!registration.isReady()) {
        return false;
    }
    std::unique_ptr<QObject> registrationObject(registration.create());
    auto *facade = engine.singletonInstance<DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    const auto loaded = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QString error;
    return registrationObject != nullptr && facade != nullptr && loaded.ok
        && facade->publish(loaded.theme, {}, &error);
}

// Least-authority stand-ins for the borrowed facades. The QML consumes only
// `places.rows`/`places.open` and the launcher sections/activate seam.
class StubPlaces final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows CONSTANT)

public:
    explicit StubPlaces(QObject *parent = nullptr) : QObject(parent) {}

    [[nodiscard]] QVariantList rows() const
    {
        return QVariantList{
            QVariantMap{{QStringLiteral("id"), QStringLiteral("home")},
                        {QStringLiteral("label"), QStringLiteral("Home")},
                        {QStringLiteral("path"), QStringLiteral("/home/fixture")},
                        {QStringLiteral("iconName"), QStringLiteral("user-home")},
                        {QStringLiteral("accessibleName"),
                         QStringLiteral("Home, /home/fixture")},
                        {QStringLiteral("index"), 0}},
            QVariantMap{{QStringLiteral("id"), QStringLiteral("desktop")},
                        {QStringLiteral("label"), QStringLiteral("Desktop")},
                        {QStringLiteral("path"), QStringLiteral("/home/fixture/Desktop")},
                        {QStringLiteral("iconName"), QStringLiteral("user-desktop")},
                        {QStringLiteral("accessibleName"),
                         QStringLiteral("Desktop, /home/fixture/Desktop")},
                        {QStringLiteral("index"), 1}},
            QVariantMap{{QStringLiteral("id"), QStringLiteral("documents")},
                        {QStringLiteral("label"), QStringLiteral("Documents")},
                        {QStringLiteral("path"), QStringLiteral("/home/fixture/Documents")},
                        {QStringLiteral("iconName"), QStringLiteral("folder-documents")},
                        {QStringLiteral("accessibleName"),
                         QStringLiteral("Documents, /home/fixture/Documents")},
                        {QStringLiteral("index"), 2}},
        };
    }

    Q_INVOKABLE bool open(const QString &placeId)
    {
        opened.append(placeId);
        return true;
    }

    QStringList opened;
};

class StubDesktopControlsAccess final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *places READ places CONSTANT)

public:
    explicit StubDesktopControlsAccess(StubPlaces *places,
                                       QObject *parent = nullptr)
        : QObject(parent), m_places(places)
    {
    }
    [[nodiscard]] QObject *places() const { return m_places; }

private:
    QObject *m_places = nullptr;
};

class StubLauncher final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)

public:
    explicit StubLauncher(QObject *parent = nullptr) : QObject(parent) {}

    [[nodiscard]] QVariantList sections() const
    {
        return QVariantList{QVariantMap{
            {QStringLiteral("kind"), QStringLiteral("pinned")},
            {QStringLiteral("identity"), QStringLiteral("pinned")},
            {QStringLiteral("items"),
             QVariantList{QVariantMap{
                 {QStringLiteral("entryId"),
                  QStringLiteral("org.qindaqt.Terminal")},
                 {QStringLiteral("displayText"), QStringLiteral("Terminal")},
                 {QStringLiteral("iconName"), QString()},
                 {QStringLiteral("accessibleDescription"),
                  QStringLiteral("Terminal")},
                 {QStringLiteral("pinned"), true}}}}}};
    }

    Q_INVOKABLE bool activate(const QString &entryId,
                              const QString &actionId = QString())
    {
        activated.append(entryId);
        Q_UNUSED(actionId);
        return true;
    }

    QStringList activated;
};

// One resolved desktop-icons applet map in the
// ResolvedAppletInstance::toVariantMap shape the controller publishes.
inline QVariantList makeApplets(const QVariantMap &settings)
{
    return QVariantList{QVariantMap{
        {QStringLiteral("id"), QStringLiteral("desktop-icons")},
        {QStringLiteral("plugin"), QStringLiteral("desktop-icons")},
        {QStringLiteral("settings"), settings},
        {QStringLiteral("runtime"),
         QVariantMap{{QStringLiteral("status"), QStringLiteral("ready")},
                     {QStringLiteral("ready"), true},
                     {QStringLiteral("displayName"),
                      QStringLiteral("Desktop Icons")},
                     {QStringLiteral("entryPoint"),
                      QStringLiteral("qindaqt.applets.desktop-icons")},
                     {QStringLiteral("hostMode"), QStringLiteral("builtin")},
                     {QStringLiteral("grantedCapabilities"), QStringList()},
                     {QStringLiteral("diagnostic"), QString()}}}}};
}

struct SurfaceHost {
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;

    bool create(QObject *access, QObject *launcherAccess,
                const QVariantMap &settings, QString *error)
    {
        engine = std::make_unique<QQmlEngine>();
        engine->addImportPath(
            QStringLiteral(QINDAQT_DESKTOP_SURFACE_QML_IMPORT_PATH));
        if (!publishTokens(*engine)) {
            *error = QStringLiteral("token publication failed");
            return false;
        }
        QQmlComponent component(engine.get());
        component.loadFromModule(QStringLiteral("QindaQt.Shell.DesktopSurface"),
                                 QStringLiteral("DesktopSurface"));
        if (!component.isReady()) {
            *error = component.errorString();
            return false;
        }
        QVariantMap initialProperties{
            {QStringLiteral("applets"), makeApplets(settings)},
            {QStringLiteral("access"), QVariant::fromValue(access)},
            {QStringLiteral("launcherAccess"),
             QVariant::fromValue(launcherAccess)},
            {QStringLiteral("screenName"), QStringLiteral("OFFSCREEN0")},
        };
        QObject *created =
            component.createWithInitialProperties(initialProperties);
        window.reset(qobject_cast<QQuickWindow *>(created));
        if (window == nullptr) {
            *error = component.errorString().isEmpty()
                ? QStringLiteral("desktop surface did not create a window")
                : component.errorString();
            delete created;
            return false;
        }
        window->setGeometry(0, 0, 800, 600);
        window->show();
        return true;
    }

    template <typename T>
    T *child(const QString &objectName) const
    {
        return window->findChild<T *>(objectName);
    }

    // Popup windows own their own QQuickWindow; visual children of a Popup
    // therefore live under the popup's contentItem. The walk stays scoped to
    // this host's window and the popup windows transient to it, so a second
    // host in the same process (or a lingering earlier scene) cannot leak
    // items into a lookup.
    QList<QQuickItem *> visualItemsNamed(const QString &name) const
    {
        QList<QQuickItem *> matches;
        const auto visit = [&matches, &name](auto &&self,
                                             QQuickItem *current) -> void {
            if (current->objectName() == name
                && !matches.contains(current)) {
                matches.append(current);
            }
            for (QQuickItem *childItem : current->childItems()) {
                self(self, childItem);
            }
        };
        for (QWindow *candidateWindow : QGuiApplication::allWindows()) {
            if (candidateWindow != window.get()
                && candidateWindow->transientParent() != window.get()) {
                continue;
            }
            auto *quickWindow = qobject_cast<QQuickWindow *>(candidateWindow);
            if (quickWindow != nullptr) {
                visit(visit, quickWindow->contentItem());
            }
        }
        if (window != nullptr) {
            for (QQuickItem *candidate :
                 window->findChildren<QQuickItem *>(name)) {
                if (!matches.contains(candidate)) {
                    matches.append(candidate);
                }
            }
        }
        return matches;
    }

    void clickWindow(Qt::MouseButton button,
                     Qt::KeyboardModifiers modifiers, const QPointF &position)
    {
        QTest::mouseMove(window.get(), position.toPoint());
        QTest::mouseClick(window.get(), button, modifiers, position.toPoint());
    }
};

} // namespace QindaQt::Tests::DesktopSurface
