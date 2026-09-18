// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/obs_client/obs_provisioning.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>

namespace QindaQt::Obs {
namespace {

constexpr int PasswordLength = 32;

QString profileDirectory(const QString &root) {
    return QDir(root).filePath(QStringLiteral("basic/profiles/%1")
                                   .arg(QLatin1String(ProfileName)));
}

QString sceneCollectionPath(const QString &root) {
    return QDir(root).filePath(QStringLiteral("basic/scenes/%1.json")
                                   .arg(QLatin1String(SceneCollectionName)));
}

QString webSocketConfigPath(const QString &root) {
    return QDir(root).filePath(
        QStringLiteral("plugin_config/obs-websocket/config.json"));
}

// AGENT-GUARD: QSaveFile, not QFile. A crash or a full disk mid-write must
// not leave OBS with a truncated profile it then refuses to load.
bool writeAtomically(const QString &path, const QByteArray &contents,
                     QString *error) {
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not create %1").arg(info.absolutePath());
        }
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write %1: %2")
                         .arg(path, file.errorString());
        }
        return false;
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Could not write %1: %2")
                         .arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

QJsonObject readJsonObject(const QString &path, bool *found) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (found != nullptr) {
            *found = false;
        }
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (found != nullptr) {
        *found = document.isObject();
    }
    return document.object();
}

// One scene item wrapping a source, in the shape OBS's scene collection
// format expects.
QJsonObject sourceEntry(const QString &name, const QString &id,
                        const QJsonObject &settings) {
    return QJsonObject{
        {QStringLiteral("name"), name},
        {QStringLiteral("id"), id},
        {QStringLiteral("versioned_id"), id},
        {QStringLiteral("settings"), settings},
        {QStringLiteral("mixers"), 255},
        {QStringLiteral("volume"), 1.0},
        {QStringLiteral("muted"), false},
        {QStringLiteral("enabled"), true},
    };
}

} // namespace

QString generateWebSocketPassword() {
    // URL-safe alphabet: obs-websocket puts this into a JSON file and a
    // user may retype it, so no quoting hazards and no ambiguous glyphs.
    static const QString alphabet = QStringLiteral(
        "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    QString password;
    password.reserve(PasswordLength);
    for (int index = 0; index < PasswordLength; ++index) {
        const quint32 pick =
            QRandomGenerator::system()->bounded(quint32(alphabet.size()));
        password.append(alphabet.at(int(pick)));
    }
    return password;
}

QJsonObject webSocketConfigDocument(const WebSocketSettings &settings) {
    // Keys read out of the installed obs-websocket.so, not guessed.
    return QJsonObject{
        {QStringLiteral("server_enabled"), settings.serverEnabled},
        {QStringLiteral("server_port"), settings.serverPort},
        {QStringLiteral("server_password"), settings.password},
        {QStringLiteral("auth_required"), settings.authRequired},
        {QStringLiteral("alerts_enabled"), settings.alertsEnabled},
        // first_load off: obs-websocket otherwise pops its own welcome
        // dialog and generates a password of its own, which would not be the
        // one in the Secret Service.
        {QStringLiteral("first_load"), false},
    };
}

QJsonObject sceneCollectionDocument() {
    const QJsonObject screenCapture = sourceEntry(
        QStringLiteral("Screen"), QStringLiteral("pipewire-desktop-capture-source"),
        QJsonObject{});
    const QJsonObject microphone = sourceEntry(
        QStringLiteral("Microphone"), QStringLiteral("pulse_input_capture"),
        QJsonObject{{QStringLiteral("device_id"), QStringLiteral("default")}});
    const QJsonObject scene = QJsonObject{
        {QStringLiteral("name"), QStringLiteral("Desktop")},
        {QStringLiteral("id"), QStringLiteral("scene")},
        {QStringLiteral("versioned_id"), QStringLiteral("scene")},
        {QStringLiteral("settings"),
         QJsonObject{
             {QStringLiteral("items"),
              QJsonArray{QJsonObject{
                  {QStringLiteral("name"), QStringLiteral("Screen")},
                  {QStringLiteral("visible"), true},
                  {QStringLiteral("locked"), false},
                  {QStringLiteral("bounds_type"), 0},
                  {QStringLiteral("scale"),
                   QJsonObject{{QStringLiteral("x"), 1.0},
                               {QStringLiteral("y"), 1.0}}},
                  {QStringLiteral("pos"),
                   QJsonObject{{QStringLiteral("x"), 0.0},
                               {QStringLiteral("y"), 0.0}}}}}}}},
        {QStringLiteral("mixers"), 0},
        {QStringLiteral("volume"), 1.0},
        {QStringLiteral("muted"), false},
        {QStringLiteral("enabled"), true},
    };
    return QJsonObject{
        {QStringLiteral("name"), QLatin1String(SceneCollectionName)},
        {QStringLiteral("current_scene"), QStringLiteral("Desktop")},
        {QStringLiteral("current_program_scene"), QStringLiteral("Desktop")},
        // AGENT-NOTE: The console buses are NOT listed here. The QindaQt OBS
        // bridge creates and removes them to match Audio1 (ADR-0190); a
        // scene collection that also declared them would fight the bridge on
        // every console change.
        {QStringLiteral("sources"),
         QJsonArray{screenCapture, microphone, scene}},
    };
}

QString profileIniDocument() {
    // Only the settings a desktop needs to record and stream sensibly. The
    // stream service and key are deliberately absent: QindaQt does not know
    // the user's channel and must not invent one.
    return QStringLiteral(
        "[General]\n"
        "Name=%1\n"
        "\n"
        "[Output]\n"
        "Mode=Simple\n"
        "\n"
        "[SimpleOutput]\n"
        "FilePath=$HOME/Videos\n"
        "RecFormat2=mkv\n"
        "RecQuality=Stream\n"
        "\n"
        "[Audio]\n"
        "SampleRate=48000\n"
        "ChannelSetup=Stereo\n"
        "\n"
        "[Video]\n"
        "FPSCommon=60\n")
        .arg(QLatin1String(ProfileName));
}

bool installProvisioning(const QString &root, const WebSocketSettings &settings,
                         QString *error) {
    if (root.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("No OBS configuration directory");
        }
        return false;
    }
    if (!settings.isValid()) {
        if (error != nullptr) {
            *error = QStringLiteral(
                "Refusing to write obs-websocket settings that would leave "
                "the server unusable");
        }
        return false;
    }
    if (!writeAtomically(
            QDir(profileDirectory(root)).filePath(QStringLiteral("basic.ini")),
            profileIniDocument().toUtf8(), error)) {
        return false;
    }
    if (!writeAtomically(
            sceneCollectionPath(root),
            QJsonDocument(sceneCollectionDocument()).toJson(QJsonDocument::Indented),
            error)) {
        return false;
    }
    return writeAtomically(
        webSocketConfigPath(root),
        QJsonDocument(webSocketConfigDocument(settings)).toJson(QJsonDocument::Indented),
        error);
}

WebSocketSettings readWebSocketSettings(const QString &root, bool *found) {
    bool present = false;
    const QJsonObject document =
        readJsonObject(webSocketConfigPath(root), &present);
    if (found != nullptr) {
        *found = present;
    }
    WebSocketSettings settings;
    if (!present) {
        // AGENT-GUARD: fail closed. The struct's own default says the server
        // is enabled, which is the right default for what we WRITE but the
        // wrong answer for what we READ: a caller that forgot to check
        // `found` would otherwise conclude an absent configuration means a
        // running server.
        settings.serverEnabled = false;
        settings.authRequired = false;
        settings.serverPort = 0;
        return settings;
    }
    settings.serverEnabled =
        document.value(QStringLiteral("server_enabled")).toBool(false);
    settings.serverPort = document.value(QStringLiteral("server_port")).toInt(0);
    settings.authRequired =
        document.value(QStringLiteral("auth_required")).toBool(false);
    settings.alertsEnabled =
        document.value(QStringLiteral("alerts_enabled")).toBool(false);
    // AGENT-GUARD: the password is deliberately not returned. A surface
    // showing the port must not come holding the secret as a side effect.
    return settings;
}

ProvisioningState inspectProvisioning(const QString &root,
                                      const WebSocketSettings &expected) {
    ProvisioningState state;
    state.profileInstalled = QFile::exists(
        QDir(profileDirectory(root)).filePath(QStringLiteral("basic.ini")));
    if (!state.profileInstalled) {
        state.problems.append(
            QStringLiteral("The QindaQt OBS profile is not installed."));
    }
    state.sceneCollectionInstalled = QFile::exists(sceneCollectionPath(root));
    if (!state.sceneCollectionInstalled) {
        state.problems.append(
            QStringLiteral("The QindaQt scene collection is not installed."));
    }
    bool found = false;
    const WebSocketSettings current = readWebSocketSettings(root, &found);
    state.webSocketConfigured = found && current.serverEnabled;
    if (!found) {
        state.problems.append(
            QStringLiteral("OBS has no obs-websocket configuration yet."));
    } else if (!current.serverEnabled) {
        state.problems.append(
            QStringLiteral("The obs-websocket server is switched off in OBS."));
    }
    state.webSocketPortMatches =
        found && current.serverPort == expected.serverPort;
    if (found && !state.webSocketPortMatches) {
        state.problems.append(
            QStringLiteral("OBS listens on port %1, not %2.")
                .arg(current.serverPort)
                .arg(expected.serverPort));
    }
    return state;
}

std::optional<QString> obsControlUrl(const WebSocketSettings &settings,
                                    const bool found) {
    if (!found || !settings.serverEnabled) {
        return std::nullopt;
    }
    const int port = settings.serverPort > 0 ? settings.serverPort : 4455;
    if (port > 65535) {
        return std::nullopt;
    }
    // Loopback only: obs-websocket has no transport security, and the
    // transport refuses anything else anyway (ADR-0198).
    return QStringLiteral("ws://127.0.0.1:%1").arg(port);
}

QString defaultObsConfigRoot() {
    const QString config = QStandardPaths::writableLocation(
        QStandardPaths::StandardLocation::ConfigLocation);
    if (config.isEmpty()) {
        return {};
    }
    return QDir(config).filePath(QStringLiteral("obs-studio"));
}

} // namespace QindaQt::Obs
