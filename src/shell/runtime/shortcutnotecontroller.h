// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QQmlComponent>
#include <QObject>
#include <QVariantMap>

#include <memory>

class QGuiApplication;
class QQmlComponent;
class QQmlEngine;
class QQuickItem;
class QQuickWindow;

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Shell {

class GlobalShortcutRegistrar;
class ShortcutNoteShortcut;

// State controller for the dismissible desktop shortcut note (ADR-0084). The
// note is presented inside the wallpaper controller's per-output background
// surfaces, so this class owns note state only: visibility persisted through a
// purpose-scoped Settings1 client, the Meta+Shift+F1 global toggle registered through
// the shared GlobalShortcutRegistrar seam, and the theme/placement values the
// note card binds to. It never creates its own window and never takes keyboard
// focus; the hosting background surfaces stay desktop-scoped and
// keyboard-inactive (ADR-0078).
//
// Threading: constructed and used on the GUI thread only. The settings client
// is borrowed and must outlive this controller. The registrar is borrowed for
// construction only; registration is synchronous.
class ShortcutNoteController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool noteVisible READ noteVisible NOTIFY noteVisibleChanged)
    Q_PROPERTY(QString primaryScreenName READ primaryScreenName NOTIFY
                   primaryScreenNameChanged)
    Q_PROPERTY(int topInset READ topInset NOTIFY insetsChanged)
    Q_PROPERTY(int rightInset READ rightInset NOTIFY insetsChanged)
    Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)

public:
    // AGENT-CONTRACT: the single Settings1 schema-v2 boolean this controller
    // reads and writes; the borrowed client must be scoped to exactly this key
    // so an absent optional key can never poison another preference scope.
    [[nodiscard]] static QString settingsKey();

    ShortcutNoteController(
        QGuiApplication &app, QQmlEngine &engine,
        Services::SettingsClient::SettingsClient &settings,
        GlobalShortcutRegistrar &registrar, QObject *parent = nullptr);

    // Production assembly: owns a purpose-scoped Settings1 client on the
    // session bus and registers through KGlobalAccel. Returned controller is
    // parented to `parent`; the settings scope travels with it.
    [[nodiscard]] static ShortcutNoteController *createProduction(
        QGuiApplication &app, QQmlEngine &engine, QObject *parent);

    [[nodiscard]] bool noteVisible() const noexcept;
    [[nodiscard]] QString primaryScreenName() const;
    [[nodiscard]] int topInset() const noexcept;
    [[nodiscard]] int rightInset() const noexcept;
    [[nodiscard]] QVariantMap theme() const;

    void setTheme(const QVariantMap &theme);
    void setPanelInsets(int top, int right);

    // Attaches one note card to a wallpaper background window. Called by
    // WallpaperController for every live output; each card is a QObject child
    // of its window and dies with it. Cards render only on the primary output.
    void attachToWindow(QQuickWindow &window, const QString &screenName);

    [[nodiscard]] ShortcutNoteShortcut *shortcut() const noexcept;

public Q_SLOTS:
    // Clickable dismissal: hides now and persists the dismissal.
    void dismiss();

    // Global-toggle path: dismisses when visible, otherwise reopens and
    // persists the reopened state so the next session matches what is shown.
    void toggle();

Q_SIGNALS:
    void noteVisibleChanged();
    void primaryScreenNameChanged();
    void insetsChanged();
    void themeChanged();

private:
    void applySnapshot();
    void persist(bool dismissed);
    bool ensureCardComponent();

    QGuiApplication &m_app;
    QQmlEngine &m_engine;
    Services::SettingsClient::SettingsClient &m_settings;
    ShortcutNoteShortcut *m_shortcut = nullptr;
    std::unique_ptr<QQmlComponent> m_cardComponent;
    QVariantMap m_theme;
    QString m_primaryScreenName;
    int m_topInset = 0;
    int m_rightInset = 0;
    bool m_noteVisible = true;
    bool m_cardComponentFailed = false;
};

} // namespace QindaQt::Shell
