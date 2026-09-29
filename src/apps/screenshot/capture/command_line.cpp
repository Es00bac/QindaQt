// SPDX-License-Identifier: GPL-3.0-or-later
#include "command_line.h"

#include <QCommandLineParser>

namespace QindaQt::Screenshot {
namespace {

struct ModeFlag {
    const char *longName;
    const char *shortName;
    CaptureMode mode;
    const char *description;
};

constexpr ModeFlag ModeFlags[] = {
    {"region", "r", CaptureMode::Region, "Select a rectangular region to capture"},
    {"fullscreen", "f", CaptureMode::AllScreens, "Capture every screen"},
    {"current-screen", "m", CaptureMode::CurrentScreen, "Capture the screen that is in use"},
    {"active", "a", CaptureMode::ActiveWindow, "Capture the active window"},
    {"window", "w", CaptureMode::WindowUnderPointer,
     "Capture the window you click (the window under the pointer)"},
};

} // namespace

Invocation parseInvocation(const QStringList &arguments)
{
    Invocation invocation;
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QindaQt Screenshot: capture the screen, a region or a window, and "
                       "start or stop an OBS recording."));
    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption version = parser.addVersionOption();
    QList<QCommandLineOption> modeOptions;
    for (const ModeFlag &flag : ModeFlags) {
        modeOptions.append(QCommandLineOption(
            {QString::fromLatin1(flag.shortName), QString::fromLatin1(flag.longName)},
            QString::fromLatin1(flag.description)));
        parser.addOption(modeOptions.constLast());
    }
    const QCommandLineOption delay({QStringLiteral("d"), QStringLiteral("delay")},
                                   QStringLiteral("Wait this many seconds (0-60) before capturing"),
                                   QStringLiteral("seconds"));
    const QCommandLineOption pointer({QStringLiteral("p"), QStringLiteral("pointer")},
                                     QStringLiteral("Include the mouse pointer"));
    const QCommandLineOption noDecorations(
        QStringLiteral("no-decorations"),
        QStringLiteral("Leave out the window title bar and borders"));
    const QCommandLineOption copy({QStringLiteral("c"), QStringLiteral("copy")},
                                  QStringLiteral("Copy the capture to the clipboard, without a window"));
    const QCommandLineOption save(
        {QStringLiteral("s"), QStringLiteral("save")},
        QStringLiteral("Save the capture without a window; an optional path or folder may follow"));
    const QCommandLineOption recordToggle(
        QStringLiteral("record-toggle"),
        QStringLiteral("Start an OBS recording, or stop the one that is running"));
    parser.addOptions({delay, pointer, noDecorations, copy, save, recordToggle});
    parser.addPositionalArgument(QStringLiteral("path"),
                                 QStringLiteral("Where --save writes (file or folder)"),
                                 QStringLiteral("[path]"));

    if (!parser.parse(arguments)) {
        invocation.error = parser.errorText();
        return invocation;
    }
    if (parser.isSet(help) || parser.isSet(version)) {
        invocation.handled = true;
        // Parsed before QCoreApplication exists, so name the program here.
        invocation.handledText =
            parser.isSet(help)
                ? parser.helpText().replace(QStringLiteral("<executable_name>"),
                                            QStringLiteral("qindaqt-screenshot"))
                : QStringLiteral("qindaqt-screenshot 0.1.0\n");
        return invocation;
    }

    int modes = 0;
    for (qsizetype index = 0; index < modeOptions.size(); ++index) {
        if (parser.isSet(modeOptions.at(index))) {
            ++modes;
            invocation.capture.mode = ModeFlags[index].mode;
        }
    }
    const bool toggling = parser.isSet(recordToggle);
    if (modes > 1 || (toggling && modes > 0)) {
        invocation.error = QStringLiteral("choose one of --region, --fullscreen, --current-screen, "
                                          "--active, --window or --record-toggle");
        return invocation;
    }
    if (parser.isSet(delay)) {
        bool ok = false;
        const int seconds = parser.value(delay).toInt(&ok);
        if (!ok || seconds < 0 || seconds > kMaxDelaySeconds) {
            invocation.error = QStringLiteral("--delay takes whole seconds from 0 to %1")
                                   .arg(kMaxDelaySeconds);
            return invocation;
        }
        invocation.capture.delaySeconds = seconds;
    }
    invocation.capture.includePointer = parser.isSet(pointer);
    invocation.capture.includeDecorations = !parser.isSet(noDecorations);
    invocation.copy = parser.isSet(copy);
    invocation.save = parser.isSet(save);

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty() && (!invocation.save || positional.size() > 1)) {
        invocation.error = QStringLiteral("a path is accepted only once, after --save");
        return invocation;
    }
    if (invocation.save && !positional.isEmpty())
        invocation.savePath = positional.constFirst();

    if (toggling) {
        if (invocation.copy || invocation.save || parser.isSet(delay)) {
            invocation.error = QStringLiteral("--record-toggle takes no capture options");
            return invocation;
        }
        invocation.action = Invocation::Action::RecordToggle;
        return invocation;
    }
    if (modes == 0) {
        if (invocation.copy || invocation.save) {
            invocation.error = QStringLiteral("--copy and --save need a capture mode such as --region");
            return invocation;
        }
        invocation.action = Invocation::Action::Window;
        return invocation;
    }
    invocation.action = Invocation::Action::Capture;
    return invocation;
}

} // namespace QindaQt::Screenshot
