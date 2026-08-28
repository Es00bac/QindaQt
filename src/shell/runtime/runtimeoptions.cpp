// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtimeoptions.h"

#include <QCommandLineParser>
#include <QCoreApplication>

#include <algorithm>
#include <limits>

namespace QindaQt::Shell {
namespace {

void configureParser(QCommandLineParser &parser)
{
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
        {QStringLiteral("compositor-pid"),
         QStringLiteral("Expected compositor process id for authenticated session state."),
         QStringLiteral("pid")},
        {QStringLiteral("development-evidence-predecessor-pid"),
         QStringLiteral("Expected prior shell owner during a bounded development restart."),
         QStringLiteral("pid")},
        {QStringLiteral("list"),
         QStringLiteral("List validated profiles, themes, and applets, then exit.")},
    });
}

bool isStrictDecimal(const QString &value)
{
    return !value.isEmpty()
        && std::all_of(value.cbegin(), value.cend(), [](QChar character) {
               return character >= QLatin1Char('0')
                   && character <= QLatin1Char('9');
           });
}

RuntimeOptionsResult optionsFromParser(const QCommandLineParser &parser)
{
    const QString tokenOption = QStringLiteral("presentation-token-fd");
    const QString compositorOption = QStringLiteral("compositor-pid");
    const QString predecessorOption =
        QStringLiteral("development-evidence-predecessor-pid");
    if (parser.values(tokenOption).size() > 1
        || parser.values(compositorOption).size() > 1
        || parser.values(predecessorOption).size() > 1) {
        return {{}, QStringLiteral("notification trust options must not be repeated")};
    }
    const bool hasTokenDescriptor = parser.isSet(tokenOption);
    const bool hasCompositorProcessId = parser.isSet(compositorOption);
    const bool hasPredecessorProcessId = parser.isSet(predecessorOption);
    // AGENT-CONTRACT: The private presenter token and compositor lineage are a
    // single authority bundle. A partial bundle must not create a presenter or
    // a misleading lock-state trust anchor.
    if (hasTokenDescriptor != hasCompositorProcessId) {
        return {{}, QStringLiteral("presentation token descriptor and compositor pid must be supplied together")};
    }
    if (hasPredecessorProcessId && !hasTokenDescriptor) {
        return {{}, QStringLiteral("development evidence predecessor requires the notification authority bundle")};
    }

    RuntimeOptions options;
    options.profileId = parser.value(QStringLiteral("profile"));
    options.themeId = parser.value(QStringLiteral("theme"));
    options.profileDirectory = parser.value(QStringLiteral("profile-dir"));
    options.themeDirectory = parser.value(QStringLiteral("theme-dir"));
    options.appletDirectory = parser.value(QStringLiteral("applet-dir"));
    options.appletPolicyFile = parser.value(QStringLiteral("applet-policy"));
    if (hasTokenDescriptor) {
        bool valid = false;
        options.presentationTokenDescriptor =
            parser.value(tokenOption).toInt(&valid);
        if (!valid || options.presentationTokenDescriptor < 3) {
            return {{}, QStringLiteral("presentation token descriptor must be an integer at least 3")};
        }

        const QString processIdText = parser.value(compositorOption);
        qint64 processId = 0;
        if (isStrictDecimal(processIdText)) {
            processId = processIdText.toLongLong(&valid, 10);
        } else {
            valid = false;
        }
        if (!valid || processId <= 1
            || processId > std::numeric_limits<qint32>::max()) {
            return {{}, QStringLiteral("compositor pid must be a decimal integer greater than 1")};
        }
        options.compositorProcessId = processId;
    }
    if (hasPredecessorProcessId) {
        bool valid = false;
        const QString processIdText = parser.value(predecessorOption);
        qint64 processId = 0;
        if (isStrictDecimal(processIdText)) {
            processId = processIdText.toLongLong(&valid, 10);
        }
        if (!valid || processId <= 1
            || processId > std::numeric_limits<qint32>::max()) {
            return {{}, QStringLiteral("development evidence predecessor pid must be a decimal integer greater than 1")};
        }
        options.developmentEvidencePredecessorProcessId = processId;
    }
    options.listOnly = parser.isSet(QStringLiteral("list"));
    return {options, {}};
}

} // namespace

RuntimeOptionsResult parseRuntimeOptions(QCoreApplication &application)
{
    QCommandLineParser parser;
    configureParser(parser);
    parser.process(application);
    return optionsFromParser(parser);
}

RuntimeOptionsResult parseRuntimeOptions(const QStringList &arguments)
{
    QCommandLineParser parser;
    configureParser(parser);
    if (!parser.parse(arguments)) {
        return {{}, parser.errorText()};
    }
    return optionsFromParser(parser);
}

} // namespace QindaQt::Shell
