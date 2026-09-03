// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QEventLoop>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QStringList>
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

namespace {

struct PinnedFontFile {
    const char *fileName;
    const char *family;
};

// AGENT-CONTRACT: The 25 Controls visual rows compare reviewed baselines that
// must render only from these repository-owned font bytes. The vendored files
// carry the repository-owned family names "QindaQt Sans"/"QindaQt Sans Mono"
// (name records rewritten by tests/controls/fonts/rename_family_names.py; the
// glyph data is byte-identical to the upstream Noto builds recorded in the
// fonts README). Because no host font can declare those families, registering
// through QFontDatabase::addApplicationFont makes them the only providers and
// the fixture cannot collide with or be shadowed by host-installed Noto,
// however Qt or fontconfig evolve their match order. Every registration must
// expose exactly the listed family; a missing, unreadable, or renamed fixture
// aborts the test binary (qFatal) with the path, because a host font package
// update would otherwise silently re-render every reviewed glyph (ADR-0021
// "Amended"). The row environment deliberately keeps the documented host
// fontconfig configuration: it supplies the rasterization parameters and the
// DejaVu fallback glyph the reviewed baselines contain, and an empty
// fontconfig configuration changes text advances (re-wrapping) and removes
// that fallback instead of pinning bytes.
constexpr PinnedFontFile kPinnedFontFiles[] = {
    {"NotoSans-Regular.ttf", "QindaQt Sans"},
    {"NotoSans-SemiBold.ttf", "QindaQt Sans"},
    {"NotoSans-Bold.ttf", "QindaQt Sans"},
    {"NotoSansMono-Regular.ttf", "QindaQt Sans Mono"},
};

} // namespace

void pinDeterministicFonts(const QString &fontDir)
{
    const QString dir = fontDir.isEmpty() ? QStringLiteral(QINDAQT_CONTROLS_FONT_DIR)
                                          : fontDir;
    for (const PinnedFontFile &pinned : kPinnedFontFiles) {
        const QString path = dir + QLatin1Char('/') + QString::fromLatin1(pinned.fileName);
        if (!QFileInfo::exists(path)) {
            qFatal("byte-pinned visual font fixture is missing: %s", qPrintable(path));
        }
        const int fontId = QFontDatabase::addApplicationFont(path);
        if (fontId < 0) {
            qFatal("could not register byte-pinned visual font: %s", qPrintable(path));
        }
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        const QString family = QString::fromLatin1(pinned.family);
        if (families.size() != 1 || families.constFirst() != family) {
            qFatal("byte-pinned visual font %s declares families [%s], expected exactly [%s]",
                   qPrintable(path),
                   qPrintable(families.join(QLatin1String(", "))),
                   qPrintable(family));
        }
    }
    QFont::insertSubstitution(QStringLiteral("Inter"), QStringLiteral("QindaQt Sans"));
    QFont::insertSubstitution(QStringLiteral("JetBrains Mono"),
                              QStringLiteral("QindaQt Sans Mono"));
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
