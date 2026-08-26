// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtimeoptions.h"

#include <QCommandLineParser>
#include <QCoreApplication>

namespace QindaQt::Shell {

RuntimeOptionsResult parseRuntimeOptions(QCoreApplication &application)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Run the production QindaQt Wayland shell surfaces"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{QStringLiteral("p"), QStringLiteral("profile")},
         QStringLiteral("Select a validated layout profile by stable id."),
         QStringLiteral("id"),
         QStringLiteral("qindaqt")},
        {{QStringLiteral("t"), QStringLiteral("theme")},
         QStringLiteral("Select a validated theme by stable id. Defaults to the profile theme."),
         QStringLiteral("id")},
        {QStringLiteral("profile-dir"),
         QStringLiteral("Read profiles from this directory."),
         QStringLiteral("path")},
        {QStringLiteral("theme-dir"),
         QStringLiteral("Read themes from this directory."),
         QStringLiteral("path")},
        {QStringLiteral("list"), QStringLiteral("List validated profiles and themes, then exit.")},
    });
    parser.process(application);

    RuntimeOptions options;
    options.profileId = parser.value(QStringLiteral("profile"));
    options.themeId = parser.value(QStringLiteral("theme"));
    options.profileDirectory = parser.value(QStringLiteral("profile-dir"));
    options.themeDirectory = parser.value(QStringLiteral("theme-dir"));
    options.listOnly = parser.isSet(QStringLiteral("list"));
    return {options, {}};
}

} // namespace QindaQt::Shell
