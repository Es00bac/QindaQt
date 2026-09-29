// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/display_protocol/display_types.h>

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QVariantList>

namespace QindaQt::DisplayClient {
class Client;
}

namespace QindaQt::Apps::SettingsAppearance {

// ADR-0286: the displays and virtual desktops Appearance offers as wallpaper
// scopes. A read-only presentation catalog: connected outputs come from the
// public Display1 client, desktop rows from the compositor through the
// composition root. It stores nothing, writes nothing, and knows no policy;
// the route model decides what a choice means.
//
// displays: enabled outputs the user can pick, in Display1 order, as
//   {stableId, name, title, ordinal, primary, assignable, x, y, width, height}
//   (`title` is "Display N", matching the Display route's numbering; ambiguous
//   ADR-0017 twins are listed but not assignable). Empty when Display1 has
//   not answered: unknown is absent, never a guessed display.
// desktops: {id, name} in compositor order; empty when unknown.
//
// Threading: GUI thread only. The attached client is borrowed and guarded.
class WallpaperTargetCatalog final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
    Q_PROPERTY(QVariantList desktops READ desktops NOTIFY desktopsChanged)

public:
    explicit WallpaperTargetCatalog(QObject *parent = nullptr);
    ~WallpaperTargetCatalog() override;

    // Follows every validated snapshot of `client` (may be null to detach).
    void attachDisplayClient(DisplayClient::Client *client);
    // Projects one validated Display1 output list; an empty list clears.
    void setDisplayOutputs(const QList<Display::Output> &outputs);
    // Rows shaped like Shell::Workspaces::WorkspaceController::rows(). Rows
    // without a usable id are skipped; an empty list means desktops unknown.
    void setDesktopRows(const QVariantList &rows);

    [[nodiscard]] QVariantList displays() const { return m_displays; }
    [[nodiscard]] QVariantList desktops() const { return m_desktops; }
    // "Display N · name" for a connected output; empty when not connected.
    Q_INVOKABLE QString displayLabel(const QString &stableId) const;
    // The desktop's name; empty when the compositor does not list it.
    Q_INVOKABLE QString desktopName(const QString &desktopId) const;

Q_SIGNALS:
    void displaysChanged();
    void desktopsChanged();

private:
    void refreshFromClient();

    QPointer<DisplayClient::Client> m_client;
    QVariantList m_displays;
    QHash<QString, QString> m_displayLabels;
    QVariantList m_desktops;
    QHash<QString, QString> m_desktopNames;
};

} // namespace QindaQt::Apps::SettingsAppearance
