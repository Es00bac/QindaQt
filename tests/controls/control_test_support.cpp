// SPDX-License-Identifier: GPL-3.0-or-later
#include "control_test_support.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSaveFile>
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
    // AGENT-CONTRACT: Themes served to Controls tests come from the pinned
    // runtime copies written by pinDeterministicFonts(), never directly from
    // data/themes/: every fontFamily/monoFontFamily value there is already a
    // registered repository-owned family. Resolving the product file instead
    // would let themes that name host families ("Noto Sans") render host
    // bytes again (P1-1).
    const QString path = QStringLiteral(QINDAQT_CONTROLS_PINNED_THEME_DIR "/") + fileName;
    if (!QFileInfo::exists(path)) {
        qFatal("pinned theme copy is missing: %s (pinDeterministicFonts() must run first)",
               qPrintable(path));
    }
    return path;
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

// AGENT-GUARD: Registered families must stay synchronized with the name-table
// rewrite table in tests/controls/fonts/rename_family_names.py and with the
// substitution targets below; a mismatch here fails registration closed.
constexpr auto kPinnedProportionalFamily = "QindaQt Sans";
constexpr auto kPinnedMonoFamily = "QindaQt Sans Mono";

void rewriteThemeField(QJsonObject &theme,
                       const QString &fileName,
                       const char *key,
                       const char *pinnedFamily,
                       QString *catalogName)
{
    const QJsonValue value = theme.value(QLatin1String(key));
    if (!value.isString() || value.toString().isEmpty()) {
        qFatal("theme catalog file %s has no string %s field",
               qPrintable(fileName),
               key);
    }
    *catalogName = value.toString();
    theme.insert(QLatin1String(key), QLatin1String(pinnedFamily));
}

// Rewrites every theme in the product catalog (data/themes/*.json) with its
// fontFamily/monoFontFamily replaced by the registered repository families
// into QINDAQT_CONTROLS_PINNED_THEME_DIR, and registers QFont substitutions
// for the original catalog names.
//
// AGENT-NOTE: QFont::insertSubstitution is only consulted when the requested
// family is NOT installed (verified against Qt 6.11: requesting "Noto Sans"
// with a substitution installed still resolves the host face). Themes that
// name host-installed families therefore cannot be redirected by substitution
// alone; the pinned copies stop the fixture from requesting any host family
// at all, while the substitutions keep covering catalogs that name families
// absent from the host (for example "Inter"). Any new family named by a
// future theme is picked up automatically, so a catalog edit cannot reopen a
// host-font bypass.
void pinThemeCatalogFamilies()
{
    const QString sourceDir = QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes");
    const QStringList catalog = QDir(sourceDir).entryList({QStringLiteral("*.json")},
                                                          QDir::Files,
                                                          QDir::Name);
    if (catalog.isEmpty()) {
        qFatal("theme catalog directory is empty or missing: %s", qPrintable(sourceDir));
    }
    if (!QDir().mkpath(QStringLiteral(QINDAQT_CONTROLS_PINNED_THEME_DIR))) {
        qFatal("could not create the pinned theme directory: "
               QINDAQT_CONTROLS_PINNED_THEME_DIR);
    }
    for (const QString &fileName : catalog) {
        QFile source(sourceDir + QLatin1Char('/') + fileName);
        if (!source.open(QIODevice::ReadOnly)) {
            qFatal("could not read theme catalog file: %s", qPrintable(source.fileName()));
        }
        QJsonParseError parseError = {};
        const QJsonDocument document =
            QJsonDocument::fromJson(source.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            qFatal("theme catalog file %s is not a JSON object: %s",
                   qPrintable(fileName),
                   qPrintable(parseError.errorString()));
        }
        QJsonObject theme = document.object();
        QString proportionalName;
        QString monoName;
        rewriteThemeField(theme, fileName, "fontFamily", kPinnedProportionalFamily, &proportionalName);
        rewriteThemeField(theme, fileName, "monoFontFamily", kPinnedMonoFamily, &monoName);
        QFont::insertSubstitution(proportionalName,
                                  QString::fromLatin1(kPinnedProportionalFamily));
        QFont::insertSubstitution(monoName, QString::fromLatin1(kPinnedMonoFamily));

        QSaveFile pinned(QStringLiteral(QINDAQT_CONTROLS_PINNED_THEME_DIR "/") + fileName);
        if (!pinned.open(QIODevice::WriteOnly)) {
            qFatal("could not write the pinned theme copy: %s",
                   qPrintable(pinned.fileName()));
        }
        pinned.write(QJsonDocument(theme).toJson(QJsonDocument::Indented));
        if (!pinned.commit()) {
            qFatal("could not commit the pinned theme copy: %s",
                   qPrintable(pinned.fileName()));
        }
    }
}

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
    pinThemeCatalogFamilies();
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
