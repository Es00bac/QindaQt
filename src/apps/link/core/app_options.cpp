// SPDX-License-Identifier: GPL-3.0-or-later
#include "app_options.h"

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>

namespace QindaQt::LinkApp {

QStringList pageIds()
{
    return {QStringLiteral("devices"), QStringLiteral("messages"), QStringLiteral("apps"),
            QStringLiteral("notifications"), QStringLiteral("input"), QStringLiteral("vault")};
}

std::optional<AppOptions> parseAppOptions(const QStringList &arguments, QString *error)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("QindaLink: your phone on the desktop."));
    // `--` ends option parsing; everything after it is positional (the reply target).
    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption page(QStringLiteral("page"), QStringLiteral("Open one page: %1.").arg(pageIds().join(QStringLiteral(", "))),
                                  QStringLiteral("name"));
    const QCommandLineOption size(QStringLiteral("size"), QStringLiteral("Window size as WxH."), QStringLiteral("WxH"));
    const QCommandLineOption fake(QStringLiteral("fake"),
                                  QStringLiteral("Use scripted data instead of the service: populated, empty, unavailable or error."),
                                  QStringLiteral("scenario"));
    const QCommandLineOption grab(QStringLiteral("grab"), QStringLiteral("Render offscreen to a PNG and exit."), QStringLiteral("png"));
    const QCommandLineOption grabAfter(QStringLiteral("grab-after"), QStringLiteral("Milliseconds before --grab writes."), QStringLiteral("ms"));
    const QCommandLineOption reply(QStringLiteral("reply"), QStringLiteral("Open the notification reply form for: -- <device> <key> <action>."));
    parser.addOptions({page, size, fake, grab, grabAfter, reply});
    parser.addPositionalArgument(QStringLiteral("target"), QStringLiteral("With --reply: device, key and action."));
    if (!parser.parse(arguments)) {
        *error = parser.errorText();
        return std::nullopt;
    }
    Q_UNUSED(help)
    AppOptions options;
    if (parser.isSet(page)) {
        options.page = parser.value(page).toLower();
        if (!pageIds().contains(options.page)) {
            *error = QStringLiteral("unknown page \"%1\"; known pages: %2").arg(options.page, pageIds().join(QStringLiteral(", ")));
            return std::nullopt;
        }
    }
    if (parser.isSet(size)) {
        const QStringList parts = parser.value(size).split(QLatin1Char('x'));
        bool okW = false;
        bool okH = false;
        const int w = parts.value(0).toInt(&okW);
        const int h = parts.value(1).toInt(&okH);
        if (parts.size() != 2 || !okW || !okH || w < 200 || h < 200 || w > 8192 || h > 8192) {
            *error = QStringLiteral("--size must be WxH between 200 and 8192");
            return std::nullopt;
        }
        options.size = QSize(w, h);
    }
    if (parser.isSet(fake)) {
        options.fakeScenario = parser.value(fake).toLower();
        const QStringList known{QStringLiteral("populated"), QStringLiteral("empty"), QStringLiteral("unavailable"),
                                QStringLiteral("error")};
        if (!known.contains(options.fakeScenario)) {
            *error = QStringLiteral("--fake must be populated, empty, unavailable or error");
            return std::nullopt;
        }
    }
    if (parser.isSet(grab)) {
        options.grabPath = parser.value(grab);
        // A render must never run against the real service: it would put personal content in a PNG.
        if (!parser.isSet(fake)) {
            *error = QStringLiteral("--grab needs --fake <scenario>");
            return std::nullopt;
        }
    }
    if (parser.isSet(grabAfter)) {
        bool ok = false;
        options.grabAfterMs = parser.value(grabAfter).toInt(&ok);
        if (!ok || options.grabAfterMs < 0 || options.grabAfterMs > 60000) {
            *error = QStringLiteral("--grab-after must be 0..60000");
            return std::nullopt;
        }
    }
    const QStringList positional = parser.positionalArguments();
    if (parser.isSet(reply)) {
        if (positional.size() != 3 || positional.at(0).isEmpty() || positional.at(1).isEmpty() || positional.at(2).isEmpty()) {
            *error = QStringLiteral("--reply needs: -- <device> <key> <action>");
            return std::nullopt;
        }
        options.mode = Mode::Reply;
        options.device = positional.at(0);
        options.key = positional.at(1);
        options.action = positional.at(2);
    } else if (!positional.isEmpty()) {
        *error = QStringLiteral("unexpected argument; use --reply -- <device> <key> <action>");
        return std::nullopt;
    }
    return options;
}

} // namespace QindaQt::LinkApp
