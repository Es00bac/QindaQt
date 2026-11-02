// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <optional>

// The ADB side of one mirror: server class path, the app_process server and the TCP forward.
//
// Ownership: AdbSession owns the `adb shell` QProcess that hosts the phone server and the
// forward it created. stop() (also run by the destructor) removes exactly that forward and
// ends exactly that process; it never touches other forwards, devices or processes.
//
// Threading: GUI thread. prepare steps block for at most a few seconds each (adb round trips).
//
// AGENT-CONTRACT: the server command line and socket name follow
// phone/mirror_server/.../Server.java and docs/wiki/reference/qindalink-mirror-v1.md.
// AGENT-GUARD: the session secret goes only to the server's stdin (`adb shell -T`, no pty, so
// it is never echoed). argv, the abstract socket name and the forward spec carry only the
// non-secret socket suffix, because /proc/<pid>/cmdline and `adb forward --list` are readable
// by other local users (threat model in the reference page).
namespace qindaqt::link_mirror {

// Development jars are pushed content-addressed: /data/local/tmp/qindalink-mirror-<16 hex of
// SHA-256>.jar, verified with sha256sum, so two mirror windows never overwrite each other.
inline constexpr const char *kDevJarDevicePrefix = "/data/local/tmp/qindalink-mirror-";
inline constexpr const char *kLinkPackage = "org.qindaqt.link";

class AdbSession : public QObject {
    Q_OBJECT

public:
    AdbSession(QString adbPath, QString serial, QObject *parent = nullptr);
    ~AdbSession() override;
    AdbSession(const AdbSession &) = delete;
    AdbSession &operator=(const AdbSession &) = delete;

    // Default adb: Portage's /usr/bin/adb, else the first adb on PATH.
    [[nodiscard]] static QString defaultAdbPath();
    // Lowercase hex from the system CSPRNG: 32 characters for the secret, 16 for the suffix.
    [[nodiscard]] static QString randomHex(int bytes);

    // Chooses the server class path: an explicit jar is pushed (content-addressed, verified);
    // otherwise the installed QindaLink APK is used in place. Returns nullopt with *error set.
    [[nodiscard]] std::optional<QString> resolveClassPath(const QString &serverJar, QString *error);
    [[nodiscard]] bool startServer(const QString &classPath, const QString &socketSuffix, const QString &secret,
                                   bool verbose, QString *error);
    [[nodiscard]] std::optional<quint16> forward(const QString &socketSuffix, QString *error);
    void stop();

    [[nodiscard]] QStringList recentServerOutput() const { return m_recent; }

Q_SIGNALS:
    void serverLine(const QString &line);
    void serverExited(int exitCode);

private:
    struct Result {
        int exitCode = -1;
        QByteArray out; // stdout only
        QString error;  // empty on success; otherwise includes stderr
    };
    [[nodiscard]] Result run(const QStringList &arguments, int timeoutMs) const;
    void readServerOutput();
    [[nodiscard]] std::optional<QString> pushDevJar(const QString &serverJar, QString *error);

    QString m_adb;
    QString m_serial;
    QProcess m_server;
    std::optional<quint16> m_forwardPort;
    QByteArray m_partialLine;
    QStringList m_recent;
};

} // namespace qindaqt::link_mirror
