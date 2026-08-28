// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QEventLoop>
#include <QFont>
#include <QFontDatabase>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTimer>

#include <memory>

namespace QindaQt::Controls::TestSupport {
namespace {

bool awaitComponent(QQmlComponent &component, QString *error)
{
    if (component.status() == QQmlComponent::Loading) {
        QEventLoop loop;
        QTimer deadline;
        deadline.setSingleShot(true);
        QObject::connect(&component,
                         &QQmlComponent::statusChanged,
                         &loop,
                         [&loop](QQmlComponent::Status status) {
                             if (status != QQmlComponent::Loading) {
                                 loop.quit();
                             }
                         });
        QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
        deadline.start(5000);
        loop.exec();
    }

    if (component.isReady()) {
        return true;
    }
    if (error != nullptr) {
        *error = component.status() == QQmlComponent::Loading
            ? QStringLiteral("timed out loading the QindaQt.Tokens test import")
            : component.errorString();
    }
    return false;
}

} // namespace

QString themePath(const QString &fileName)
{
    return QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + fileName;
}

bool publishTheme(QQmlEngine &engine,
                  const QString &fileName,
                  const QindaQt::DesignTokens::AccessibilityInputs &inputs,
                  QString *error)
{
    engine.addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));

    // AGENT-NOTE: Import once before asking the engine for the singleton. This
    // exercises the generated plugin path instead of relying on static type
    // registration accidentally linked into a test executable.
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:token-registration.qml")));
    if (!awaitComponent(registration, error)) {
        return false;
    }
    std::unique_ptr<QObject> registrationObject(registration.create());
    if (!registrationObject) {
        if (error) {
            *error = registration.errorString();
        }
        return false;
    }

    auto *facade = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    if (!facade) {
        if (error) {
            *error = QStringLiteral("QindaQt.Tokens singleton was not registered");
        }
        return false;
    }

    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(themePath(fileName));
    if (!loaded.ok) {
        if (error) {
            *error = loaded.error;
        }
        return false;
    }
    return facade->publish(loaded.theme, inputs, error);
}

void pinDeterministicFonts()
{
    // AGENT-CONTRACT: Visual fixtures resolve the schema's Inter/JetBrains
    // names to fonts present in the documented test image. This keeps theme
    // identity in QST while preventing host font-install order from changing
    // glyph rasterization.
    QFont::insertSubstitution(QStringLiteral("Inter"), QStringLiteral("Noto Sans"));
    QFont::insertSubstitution(QStringLiteral("JetBrains Mono"),
                              QStringLiteral("Noto Sans Mono"));
    QLocale::setDefault(QLocale::c());
}

QColor objectColor(QObject *object)
{
    return object->property("color").value<QColor>();
}

QObject *controlBackground(QObject *control)
{
    auto *background = control->property("background").value<QObject *>();
    if (!background) {
        qFatal("control has no background object: %s", qPrintable(control->objectName()));
    }
    return background;
}

QAccessibleInterface *accessible(QObject *object)
{
    auto *interface = QAccessible::queryAccessibleInterface(object);
    if (!interface) {
        qFatal("missing accessible interface for %s", qPrintable(object->objectName()));
    }
    return interface;
}

QQuickItem *item(QQuickItem *root, const char *name)
{
    auto *result = root->findChild<QQuickItem *>(QString::fromLatin1(name));
    if (!result) {
        qFatal("missing test item: %s", name);
    }
    return result;
}

QVariantMap completePreviewUsing(const QVariant &role)
{
    const QVariantMap background = {{QStringLiteral("base"), role},
                                    {QStringLiteral("raised"), role}};
    return {{QStringLiteral("bg"), background},
            {QStringLiteral("accent"),
             QVariantMap{{QStringLiteral("default"), role}}},
            {QStringLiteral("fg"), QVariantMap{{QStringLiteral("default"), role}}},
            {QStringLiteral("outline"),
             QVariantMap{{QStringLiteral("strong"), role}}}};
}

void waitForMotion(QObject *control)
{
    QEventLoop loop;
    QTimer::singleShot(control->property("transitionDuration").toInt() + 30,
                       &loop,
                       &QEventLoop::quit);
    loop.exec();
}

} // namespace QindaQt::Controls::TestSupport
