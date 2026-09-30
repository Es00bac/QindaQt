// SPDX-License-Identifier: GPL-3.0-or-later
// Private nonprivileged native argv relay. Qt startDetached ignores the child
// modifier, so it cannot preserve an explicitly selected CLOEXEC Wayland FD.
// This one-use helper carries that owned FD through a normal double fork/exec;
// it never draws, resolves application identity, reads stores or logs payloads.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QDir>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <vector>
extern char **environ;
namespace {
bool launch(const QJsonObject &object) {
    const auto program = object.value(QStringLiteral("program")).toString();
    const auto directory = object.value(QStringLiteral("directory")).toString();
    const auto arguments = object.value(QStringLiteral("arguments")).toArray();
    if (object.size() != 3 || !QDir::isAbsolutePath(program) || program.size() > 4096
        || program.contains(QChar::Null) || directory.contains(QChar::Null) || directory.size() > 4096 || arguments.size() > 63
        || (!directory.isEmpty() && !QDir::isAbsolutePath(directory))) return false;
    std::vector<QByteArray> bytes; bytes.push_back(program.toUtf8()); qsizetype total = 0;
    for (const auto &argument : arguments) {
        if (!argument.isString()) return false;
        const auto value = argument.toString(); if (value.contains(QChar::Null)) return false;
        total += value.size(); if (total > 8192) return false; bytes.push_back(value.toUtf8());
    }
    std::vector<char *> argv; for (auto &value : bytes) argv.push_back(value.data()); argv.push_back(nullptr);
    const auto cwd = directory.toUtf8(); const auto executable = program.toUtf8();
    const bool changeDirectory = !cwd.isEmpty();
    const char *directoryBytes = cwd.constData(); const char *programBytes = executable.constData();
    char **argumentBytes = argv.data();
    int errors[2]; if (pipe2(errors, O_CLOEXEC) != 0) return false;
    const pid_t child = fork();
    if (child == 0) {
        close(errors[0]);
        auto fail = [&] { const int code = errno ? errno : EINVAL; write(errors[1], &code, sizeof(code)); _exit(2); };
        if (setsid() < 0) fail();
        const pid_t grandchild = fork(); if (grandchild < 0) fail(); if (grandchild > 0) _exit(0);
        if (changeDirectory && chdir(directoryBytes) != 0) fail();
        const int null = open("/dev/null", O_RDWR | O_CLOEXEC); if (null < 0) fail();
        if (dup2(null, STDIN_FILENO) < 0 || dup2(null, STDOUT_FILENO) < 0 || dup2(null, STDERR_FILENO) < 0) fail();
        if (null > STDERR_FILENO) close(null);
        execve(programBytes, argumentBytes, environ); fail();
    }
    close(errors[1]);
    if (child < 0) { close(errors[0]); return false; }
    int status = 0; while (waitpid(child, &status, 0) < 0) if (errno != EINTR) break;
    pollfd receipt{errors[0], POLLIN | POLLHUP, 0};
    int error = 0; const bool ready = poll(&receipt, 1, 2000) > 0;
    const bool success = ready && read(errors[0], &error, sizeof(error)) == 0;
    close(errors[0]); return success;
}
}
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0 || prctl(PR_SET_DUMPABLE, 0) != 0) return 2;
    signal(SIGPIPE, SIG_IGN); QCoreApplication app(argc, argv);
    bool valid = false; const int display = qEnvironmentVariableIntValue("WAYLAND_SOCKET", &valid);
    ucred peer{}; socklen_t size = sizeof(peer);
    if (!valid || display < 3 || getsockopt(display, SOL_SOCKET, SO_PEERCRED, &peer, &size) != 0
        || peer.uid != geteuid() || peer.pid <= 0) return 2;
    QByteArray bytes; QElapsedTimer timer; timer.start();
    while (bytes.size() <= 65536) {
        pollfd input{STDIN_FILENO, POLLIN, 0}; const auto remaining = 2000 - timer.elapsed();
        if (remaining <= 0 || poll(&input, 1, static_cast<int>(remaining)) <= 0) return 2;
        char buffer[4096]; const auto count = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count < 0) return 2;
        if (count == 0) break;
        bytes.append(buffer, static_cast<qsizetype>(count));
    }
    QJsonParseError error; const auto document = QJsonDocument::fromJson(bytes, &error);
    const bool success = bytes.size() <= 65536 && error.error == QJsonParseError::NoError && launch(document.object());
    const char *reply = success ? "1\n" : "0\n"; if (write(STDOUT_FILENO, reply, 2) != 2) return 2;
    return success ? 0 : 2;
}
