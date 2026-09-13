// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Apps::SettingsCustomize {

// Read-only projection of the Settings1 wallpaper preference pair
// (appearance.wallpaper / appearance.wallpaperMode) onto the Customize
// canvas. Owns no Settings1 authority: it consumes an injected client already
// scoped to the two keys, resolves the stored identity through the public
// Settings Appearance wallpaper catalog contract, and never writes. Missing,
// mistyped, unknown-mode, or unresolvable truth fails closed to an empty
// source so the canvas keeps its explicit token-gradient fallback; the
// stored preference itself is never renamed or repaired here.
class CustomizeWallpaperPreview final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)

public:
    // AGENT-CONTRACT: client and the wallpaper search roots must outlive this
    // GUI-thread object. The client is expected to be scoped to exactly the
    // two wallpaper keys; a wider scope is tolerated but only those keys are
    // read. Resolution mirrors the shell's contract: qindaqt:<basename>
    // identities resolve through the first readable root, anything else must
    // be an absolute readable file.
    explicit CustomizeWallpaperPreview(
        Services::SettingsClient::SettingsClient &client,
        QStringList wallpaperRoots, QObject *parent = nullptr);

    [[nodiscard]] QUrl source() const { return m_source; }
    [[nodiscard]] QString mode() const { return m_mode; }
    [[nodiscard]] QString status() const { return m_status; }

Q_SIGNALS:
    void changed();

private:
    void recompute();
    [[nodiscard]] QString resolvePreference(const QString &preference) const;

    Services::SettingsClient::SettingsClient &m_client;
    QStringList m_roots;
    QUrl m_source;
    QString m_mode{QStringLiteral("scaled")};
    QString m_status{QStringLiteral("unavailable")};
};

} // namespace QindaQt::Apps::SettingsCustomize
