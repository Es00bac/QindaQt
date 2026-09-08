// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwincommandbuilder.h"
#include "sessioncommandline.h"
#include "sessiondefaults.h"
#include "sessionenvironment.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>

#include <cerrno>
#include <cstring>
#include <optional>
#include <unistd.h>
#include <vector>

namespace {

std::optional<QindaQt::Session::KWinCommandCapabilities> queryKWinCapabilities(
    const QString &executable,
    QString *error)
{
    QProcess probe;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    probe.setProcessEnvironment(environment);
    probe.start(executable, {QStringLiteral("--help")}, QIODevice::ReadOnly);
    if (!probe.waitForStarted(5000)) {
        *error = QStringLiteral("could not inspect %1 command options: %2")
                     .arg(executable, probe.errorString());
        return std::nullopt;
    }
    if (!probe.waitForFinished(5000)) {
        probe.kill();
        probe.waitForFinished();
        *error = QStringLiteral("timed out inspecting %1 command options").arg(executable);
        return std::nullopt;
    }
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        *error = QStringLiteral("%1 --help failed with exit code %2")
                     .arg(executable)
                     .arg(probe.exitCode());
        return std::nullopt;
    }

    const auto helpText = QString::fromUtf8(probe.readAllStandardOutput())
        + QString::fromUtf8(probe.readAllStandardError());
    return QindaQt::Session::KWinCommandBuilder::capabilitiesFromHelpText(helpText);
}

int replaceWithKWin(const QStringList &command)
{
    std::vector<QByteArray> encoded;
    encoded.reserve(static_cast<std::size_t>(command.size()));
    for (const auto &argument : command) {
        encoded.push_back(QFile::encodeName(argument));
    }

    std::vector<char *> arguments;
    arguments.reserve(encoded.size() + 1);
    for (auto &argument : encoded) {
        arguments.push_back(argument.data());
    }
    arguments.push_back(nullptr);

    // AGENT-GUARD: Replacing the launcher keeps the compositor as the display
    // manager's session leader, so termination and crash reporting target the
    // real authority instead of an orphaned grandchild.
    ::execvp(arguments.front(), arguments.data());
    QTextStream(stderr) << "qindaqt-wm: could not launch " << command.constFirst() << ": "
                        << std::strerror(errno) << '\n';
    return 127;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-wm"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));

    if (application.arguments().contains(QStringLiteral("--help"))
        || application.arguments().contains(QStringLiteral("-h"))) {
        QTextStream(stdout) << QindaQt::Session::SessionCommandLine::helpText();
        return 0;
    }
    if (application.arguments().contains(QStringLiteral("--version"))) {
        QTextStream(stdout) << QCoreApplication::applicationName() << ' '
                            << QCoreApplication::applicationVersion() << '\n';
        return 0;
    }

    QString error;
    const auto options = QindaQt::Session::SessionCommandLine::parse(application.arguments(),
                                                                    &error);
    if (!options) {
        QTextStream(stderr) << "qindaqt-wm: " << error << '\n';
        return 2;
    }
    QindaQt::Session::KWinCommandCapabilities capabilities;
    if (!options->lockscreen) {
        const auto detected = queryKWinCapabilities(options->kwinExecutable, &error);
        if (!detected) {
            QTextStream(stderr) << "qindaqt-wm: " << error << '\n';
            return 2;
        }
        capabilities = *detected;
        if (capabilities.lockscreenOption && !capabilities.noLockscreenOption) {
            QTextStream(stderr)
                << "qindaqt-wm: selected KWin cannot disable its compiled-in screen locker\n";
            return 2;
        }
    }
    const auto command = QindaQt::Session::KWinCommandBuilder::build(*options,
                                                                     &error,
                                                                     capabilities);
    if (command.isEmpty()) {
        QTextStream(stderr) << "qindaqt-wm: " << error << '\n';
        return 2;
    }

    QindaQt::Session::SessionEnvironment::apply(*options);
    auto configHome = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (configHome.isEmpty()) {
        configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    }
    if (!QindaQt::Session::SessionDefaults::ensure(configHome, &error)) {
        QTextStream(stderr) << "qindaqt-wm: " << error << '\n';
        return 2;
    }
    return replaceWithKWin(command);
}
