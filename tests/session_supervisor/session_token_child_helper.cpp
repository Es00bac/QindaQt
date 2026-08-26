// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_token_channel.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

using namespace QindaQt::Services::NotificationPresentation;

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addOptions({
        {QStringLiteral("presentation-token-fd"), QStringLiteral("Token descriptor"),
         QStringLiteral("descriptor")},
        {QStringLiteral("profile"), QStringLiteral("Profile marker"),
         QStringLiteral("id")},
        {QStringLiteral("theme"), QStringLiteral("Theme marker"),
         QStringLiteral("id")},
        {QStringLiteral("quick-exit"), QStringLiteral("Exit immediately")},
    });
    if (!parser.parse(application.arguments())) {
        return 2;
    }
    bool valid = false;
    const int descriptor =
        parser.value(QStringLiteral("presentation-token-fd")).toInt(&valid);
    if (!valid || descriptor < 3) {
        return 3;
    }
    const auto result = PresentationTokenChannel::readAndClose(descriptor);
    if (!result.ok()) {
        return 4;
    }
    const bool shellRole = parser.isSet(QStringLiteral("profile"));
    QTextStream(stdout) << "token-channel-ok "
                        << (shellRole ? "shell" : "host") << '\n';
    const int lifetime = parser.isSet(QStringLiteral("quick-exit")) || shellRole
        ? 20
        : 5'000;
    QTimer::singleShot(lifetime, &application, &QCoreApplication::quit);
    return application.exec();
}
