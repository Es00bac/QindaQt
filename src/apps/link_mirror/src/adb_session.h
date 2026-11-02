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
namespace qindaqt::link_mirror {

inline constexpr const char *kDevJarDevicePath = "/data/local/tmp/qindalink-mirror-dev.jar";
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
    // 32 lowercase hex characters from the system CSPRNG.
    [[nodiscard]] static QString newToken();

    // Chooses the server class path: an explicit jar is pushed to kDevJarDevicePath;
    // otherwise the installed QindaLink APK is used in place. Returns nullopt with *error set.
    [[nodiscard]] std::optional<QString> resolveClassPath(const QString &serverJar, QString *error);
    [[nodiscard]] bool startServer(const QString &classPath, const QString &token, bool verbose, QString *error);
    [[nodiscard]] std::optional<quint16> forward(const QString &token, QString *error);
    void stop();

    [[nodiscard]] QStringList recentServerOutput() const { return m_recent; }

Q_SIGNALS:
    void serverLine(const QString &line);
    void serverExited(int exitCode);

private:
    struct Result {
        int exitCode = -1;
        QByteArray out;
        QString error;
    };
    [[nodiscard]] Result run(const QStringList &arguments, int timeoutMs) const;
    void readServerOutput();

    QString m_adb;
    QString m_serial;
    QProcess m_server;
    std::optional<quint16> m_forwardPort;
    QByteArray m_partialLine;
    QStringList m_recent;
};

} // namespace qindaqt::link_mirror
