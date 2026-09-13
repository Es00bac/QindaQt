// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/decoration_painter/decoration_painter.h"
#include "qindaqt/themes/theme_spec.h"

#include <QColor>
#include <QFont>
#include <QObject>
#include <QStringList>
#include <QVariant>

class StubWindowDecorationSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList decorations MEMBER decorations CONSTANT)
    Q_PROPERTY(QString configuredId MEMBER configuredId NOTIFY stateChanged)
    Q_PROPERTY(QString configuredName MEMBER configuredName NOTIFY stateChanged)
    Q_PROPERTY(QString selectedId MEMBER selectedId NOTIFY stateChanged)
    Q_PROPERTY(bool selectedUsesQindaQt READ selectedUsesQindaQt NOTIFY stateChanged)
    Q_PROPERTY(bool applyAvailable READ applyAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText MEMBER errorText NOTIFY stateChanged)

public:
    explicit StubWindowDecorationSettings(QObject *parent = nullptr)
        : QObject(parent)
    {
        decorations = {
            QVariantMap{{QStringLiteral("id"), QStringLiteral("native:org.qindaqt")},
                        {QStringLiteral("name"), QStringLiteral("QindaQt")},
                        {QStringLiteral("kind"), QStringLiteral("native")}},
            QVariantMap{{QStringLiteral("id"), QStringLiteral("aurorae:Scratchy")},
                        {QStringLiteral("name"), QStringLiteral("Scratchy")},
                        {QStringLiteral("kind"), QStringLiteral("aurorae")}},
        };
    }

    [[nodiscard]] bool selectedUsesQindaQt() const
    {
        return selectedId == QLatin1String("native:org.qindaqt");
    }
    [[nodiscard]] bool applyAvailable() const
    {
        return selectedId != configuredId;
    }
    [[nodiscard]] QString statusText() const
    {
        return QStringLiteral("Current window decoration: %1").arg(configuredName);
    }
    Q_INVOKABLE bool selectDecoration(const QString &id)
    {
        selectedId = id;
        Q_EMIT stateChanged();
        return true;
    }
    Q_INVOKABLE bool applySelection()
    {
        configuredId = selectedId;
        configuredName = selectedUsesQindaQt() ? QStringLiteral("QindaQt")
                                               : QStringLiteral("Scratchy");
        ++applies;
        Q_EMIT stateChanged();
        return true;
    }

    QVariantList decorations;
    QString configuredId = QStringLiteral("native:org.qindaqt");
    QString configuredName = QStringLiteral("QindaQt");
    QString selectedId = configuredId;
    QString errorText;
    int applies = 0;

Q_SIGNALS:
    void stateChanged();
};

// Duck-typed stand-in for AppearanceSettingsModel: the page is defined
// against this property surface, so the presentation test can drive every
// route without a live Settings1 lineage.
class StubAppearanceModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading MEMBER loading NOTIFY stateChanged)
    Q_PROPERTY(bool ready MEMBER ready NOTIFY stateChanged)
    Q_PROPERTY(bool saving MEMBER saving NOTIFY stateChanged)
    Q_PROPERTY(bool conflict MEMBER conflict NOTIFY stateChanged)
    Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY stateChanged)
    Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY stateChanged)
    Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY draftChanged)
    Q_PROPERTY(bool draftValid MEMBER draftValid NOTIFY draftChanged)
    Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString statusText MEMBER statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText MEMBER errorText NOTIFY stateChanged)
    Q_PROPERTY(QString saveResultsText MEMBER saveResultsText NOTIFY stateChanged)
    Q_PROPERTY(bool saveResultsHaveFailure MEMBER saveResultsHaveFailure
                   NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap draft MEMBER draft NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap fieldErrors MEMBER fieldErrors NOTIFY draftChanged)
    Q_PROPERTY(QVariantList installedThemes MEMBER installedThemes CONSTANT)
    Q_PROPERTY(QVariantList bundledWallpapers MEMBER bundledWallpapers CONSTANT)
    Q_PROPERTY(QString resolvedThemeId MEMBER resolvedThemeId NOTIFY draftChanged)
    Q_PROPERTY(bool configuredThemeInstalled MEMBER configuredThemeInstalled
                   NOTIFY draftChanged)
    Q_PROPERTY(QString fallbackNotice MEMBER fallbackNotice NOTIFY draftChanged)
    Q_PROPERTY(QVariantList previewQtPalette MEMBER previewQtPalette
                   NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap previewChrome MEMBER previewChrome NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap previewToolkitPalette MEMBER previewToolkitPalette
                   NOTIFY draftChanged)
    Q_PROPERTY(QFont previewToolkitFont MEMBER previewToolkitFont NOTIFY draftChanged)
    Q_PROPERTY(QColor previewCanvasColor MEMBER previewCanvasColor NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap previewContainerStyle MEMBER previewContainerStyle
                   NOTIFY draftChanged)
    Q_PROPERTY(QObject *windowDecorationSettings READ windowDecorationSettings CONSTANT)

public:
    explicit StubAppearanceModel(QObject *parent = nullptr) : QObject(parent) {}

    [[nodiscard]] QObject *windowDecorationSettings()
    {
        return &windowDecorations;
    }

    Q_INVOKABLE bool setDraftValue(const QString &key, const QVariant &value)
    {
        draftKeys.append(key);
        draftValues.append(value);
        draft.insert(key, value);
        Q_EMIT draftChanged();
        return true;
    }
    Q_INVOKABLE bool cancelDraft()
    {
        ++cancels;
        Q_EMIT draftChanged();
        return true;
    }
    Q_INVOKABLE bool applyDraft()
    {
        ++applies;
        return true;
    }
    Q_INVOKABLE void retry() { ++retries; }

    void publish()
    {
        Q_EMIT stateChanged();
        Q_EMIT draftChanged();
    }

    bool loading = true;
    bool ready = false;
    bool saving = false;
    bool conflict = false;
    bool unavailable = false;
    bool canEdit = false;
    bool draftDirty = false;
    bool draftValid = true;
    bool applyAvailable = false;
    bool configuredThemeInstalled = true;
    QString statusText = QStringLiteral("Loading appearance settings…");
    QString errorText;
    QString saveResultsText;
    bool saveResultsHaveFailure = false;
    QString resolvedThemeId = QStringLiteral("qinda-dark");
    QString fallbackNotice;
    QVariantList previewQtPalette;
    QVariantMap previewChrome;
    QVariantMap previewToolkitPalette;
    QFont previewToolkitFont;
    QColor previewCanvasColor;
    QVariantMap previewContainerStyle;
    QVariantMap draft;
    QVariantMap fieldErrors;
    QVariantList installedThemes;
    QVariantList bundledWallpapers;
    QStringList draftKeys;
    QVariantList draftValues;
    int applies = 0;
    int cancels = 0;
    int retries = 0;
    StubWindowDecorationSettings windowDecorations{this};

Q_SIGNALS:
    void stateChanged();
    void draftChanged();
};

// Publishes the chrome and toolkit projections the route model derives for
// `theme` with default arrangement (ADR-0127, ADR-0129), so the previews
// paint an actual theme through their real renderers.
inline void publishResolvedChrome(StubAppearanceModel &model,
                                  const QindaQt::Themes::ThemeSpec &theme)
{
    const QindaQt::Decoration::ChromePreferences preferences;
    const auto color = [&theme](const char *key) {
        return theme.colors.value(QString::fromLatin1(key));
    };
    model.previewChrome =
        QindaQt::Decoration::resolveWindowChrome(theme, preferences).toVariantMap();
    model.previewContainerStyle = QindaQt::Decoration::containerStyleToVariantMap(
        QindaQt::Decoration::resolveContainerStyle(theme, preferences));
    model.previewToolkitPalette = {
        {QStringLiteral("window"), color("surface")},
        {QStringLiteral("windowText"), color("text")},
        {QStringLiteral("base"), color("surfaceRaised")},
        {QStringLiteral("text"), color("text")},
        {QStringLiteral("button"), color("surfaceRaised")},
        {QStringLiteral("buttonText"), color("text")},
        {QStringLiteral("highlight"), color("accent")},
        {QStringLiteral("highlightedText"), color("accentText")},
        {QStringLiteral("placeholderText"), color("textMuted")}};
    model.previewCanvasColor = color("canvas");
}
