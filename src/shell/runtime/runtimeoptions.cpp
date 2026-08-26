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
        {QStringLiteral("applet-dir"),
         QStringLiteral("Read applet manifests from this directory."),
         QStringLiteral("path")},
        {QStringLiteral("applet-policy"),
         QStringLiteral("Read the applet capability policy from this file."),
         QStringLiteral("path")},
        {QStringLiteral("presentation-token-fd"),
         QStringLiteral("Consume the private notification token from this inherited descriptor."),
         QStringLiteral("descriptor")},
        {QStringLiteral("list"),
         QStringLiteral("List validated profiles, themes, and applets, then exit.")},
    });
    parser.process(application);

    RuntimeOptions options;
    options.profileId = parser.value(QStringLiteral("profile"));
    options.themeId = parser.value(QStringLiteral("theme"));
    options.profileDirectory = parser.value(QStringLiteral("profile-dir"));
    options.themeDirectory = parser.value(QStringLiteral("theme-dir"));
    options.appletDirectory = parser.value(QStringLiteral("applet-dir"));
    options.appletPolicyFile = parser.value(QStringLiteral("applet-policy"));
    if (parser.isSet(QStringLiteral("presentation-token-fd"))) {
        bool valid = false;
        options.presentationTokenDescriptor =
            parser.value(QStringLiteral("presentation-token-fd")).toInt(&valid);
        if (!valid || options.presentationTokenDescriptor < 3) {
            return {{}, QStringLiteral("presentation token descriptor must be an integer at least 3")};
        }
    }
    options.listOnly = parser.isSet(QStringLiteral("list"));
    return {options, {}};
}

} // namespace QindaQt::Shell
