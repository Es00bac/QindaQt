// SPDX-License-Identifier: GPL-3.0-or-later
#include "previewoptions.h"

#include <QCommandLineParser>
#include <QCoreApplication>

namespace QindaQt::Shell {

bool PreviewOptions::capturesScreenshot() const
{
    return !screenshotPath.isEmpty();
}

PreviewOptionsResult parsePreviewOptions(QCoreApplication &application)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Preview QindaQt layout profiles and themes"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {{QStringLiteral("p"), QStringLiteral("profile")},
         QStringLiteral("Select a layout profile by stable id."),
         QStringLiteral("id"),
         QStringLiteral("qindaqt")},
        {{QStringLiteral("t"), QStringLiteral("theme")},
         QStringLiteral("Select a theme by stable id. Defaults to the profile theme."),
         QStringLiteral("id")},
        {QStringLiteral("profile-dir"),
         QStringLiteral("Read profiles from this directory."),
         QStringLiteral("path")},
        {QStringLiteral("theme-dir"),
         QStringLiteral("Read themes from this directory."),
         QStringLiteral("path")},
        {QStringLiteral("list"), QStringLiteral("List validated profiles and themes, then exit.")},
        {QStringLiteral("width"),
         QStringLiteral("Preview width in pixels."),
         QStringLiteral("pixels"),
         QStringLiteral("1280")},
        {QStringLiteral("height"),
         QStringLiteral("Preview height in pixels."),
         QStringLiteral("pixels"),
         QStringLiteral("720")},
        {QStringLiteral("screenshot"),
         QStringLiteral("Render one deterministic frame to a PNG and exit."),
         QStringLiteral("path")},
    });
    parser.process(application);

    bool widthIsValid = false;
    bool heightIsValid = false;
    const int width = parser.value(QStringLiteral("width")).toInt(&widthIsValid);
    const int height = parser.value(QStringLiteral("height")).toInt(&heightIsValid);
    if (!widthIsValid || !heightIsValid || width < 640 || height < 480) {
        return {{}, QStringLiteral("Preview dimensions must be integers of at least 640x480")};
    }

    PreviewOptions options;
    options.profileId = parser.value(QStringLiteral("profile"));
    options.themeId = parser.value(QStringLiteral("theme"));
    options.profileDirectory = parser.value(QStringLiteral("profile-dir"));
    options.themeDirectory = parser.value(QStringLiteral("theme-dir"));
    options.screenshotPath = parser.value(QStringLiteral("screenshot"));
    options.width = width;
    options.height = height;
    options.listOnly = parser.isSet(QStringLiteral("list"));
    return {options, {}};
}

} // namespace QindaQt::Shell
