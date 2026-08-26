// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwincommandbuilder.h"
#include "sessioncommandline.h"
#include "sessiondefaults.h"
#include "sessionenvironment.h"

#include <QCoreApplication>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace {

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
    const auto command = QindaQt::Session::KWinCommandBuilder::build(*options, &error);
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
