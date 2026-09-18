// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>

#include <optional>
#include <QStringList>

namespace QindaQt::Obs {

// What QindaQt writes into OBS's own configuration so the desktop's
// streaming controls work on a fresh machine, and how it checks that it is
// still there.
//
// AGENT-CONTRACT: This is provisioning, not ownership. QindaQt writes ONE
// profile and ONE scene collection, both named "QindaQt", and the
// obs-websocket server settings. It never edits a profile or collection the
// user made, never renames theirs, and never switches OBS to ours behind
// their back — `install()` writes files, and OBS's own UI decides what is
// current.
//
// AGENT-GUARD: Every path is taken from the injected root, so a test writes
// into a temporary directory and never near the user's real OBS.

// The websocket server settings a QindaQt desktop needs.
//
// The key names are obs-websocket's own, read out of the installed
// `obs-websocket.so` (server_enabled / server_port / server_password /
// auth_required / alerts_enabled / first_load), not guessed.
struct WebSocketSettings {
    bool serverEnabled = true;
    int serverPort = 4455;
    QString password;
    bool authRequired = true;
    bool alertsEnabled = false;

    [[nodiscard]] bool isValid() const {
        return serverPort > 0 && serverPort <= 65535 &&
               (!authRequired || !password.isEmpty());
    }
};

// What a check found. `installed` means every file QindaQt writes is present
// and readable; `matches` means their content is what this version writes.
// A surface says "install" for the first and "repair" for the second,
// because they are different promises.
struct ProvisioningState {
    bool profileInstalled = false;
    bool sceneCollectionInstalled = false;
    bool webSocketConfigured = false;
    bool webSocketPortMatches = false;
    QStringList problems;

    [[nodiscard]] bool complete() const {
        return profileInstalled && sceneCollectionInstalled &&
               webSocketConfigured && webSocketPortMatches;
    }
};

// The name QindaQt gives its profile and its scene collection. One name, so
// a repair can find what a previous install wrote.
inline constexpr char ProfileName[] = "QindaQt";
inline constexpr char SceneCollectionName[] = "QindaQt";

// A password worth generating: 32 URL-safe characters from a cryptographic
// source. obs-websocket compares it verbatim, so length is the only defence.
[[nodiscard]] QString generateWebSocketPassword();

// The documents, as pure values.
[[nodiscard]] QJsonObject webSocketConfigDocument(const WebSocketSettings &settings);
// The scene collection: a desktop capture through the portal, the default
// microphone, and nothing that pretends to know the user's stream key.
[[nodiscard]] QJsonObject sceneCollectionDocument();
// The profile's `basic.ini`, as text: OBS reads an ini here, not JSON.
[[nodiscard]] QString profileIniDocument();

// Writes profile, scene collection and websocket settings under `root`
// (normally `~/.config/obs-studio`). Returns false with a diagnostic and
// leaves no half-written file behind; an existing QindaQt profile is
// replaced, a user's own profile is never touched.
[[nodiscard]] bool installProvisioning(const QString &root,
                                       const WebSocketSettings &settings,
                                       QString *error);

// Reads back what is there. Never writes.
[[nodiscard]] ProvisioningState inspectProvisioning(const QString &root,
                                                    const WebSocketSettings &expected);

// The websocket settings currently in OBS's config under `root`, with the
// password left out — a surface must be able to show the port and whether
// authentication is on without holding the secret.
[[nodiscard]] WebSocketSettings readWebSocketSettings(const QString &root,
                                                      bool *found = nullptr);

// `$XDG_CONFIG_HOME/obs-studio`, or the documented fallback.
[[nodiscard]] QString defaultObsConfigRoot();

// The loopback URL to connect to, or nothing while OBS is not set up for
// control yet.
//
// AGENT-CONTRACT: this is the FIRST gate a connecting surface applies, and it
// reads only OBS's own configuration. A caller must not ask the Secret Service
// for the password until this returns a URL, so a desktop with no OBS never
// generates keyring traffic however long it runs. `found` is the out-parameter
// from readWebSocketSettings; a config that is absent or has the server
// disabled yields nothing.
[[nodiscard]] std::optional<QString>
obsControlUrl(const WebSocketSettings &settings, bool found);

} // namespace QindaQt::Obs
