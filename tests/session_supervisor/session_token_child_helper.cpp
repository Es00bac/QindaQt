// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/notification_presentation/presentation_token_channel.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

using namespace QindaQt::Services::NotificationPresentation;

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addOptions({
        {QStringLiteral("presentation-token-fd"), QStringLiteral("Token descriptor"),
         QStringLiteral("descriptor")},
        {QStringLiteral("compositor-pid"), QStringLiteral("Compositor pid"),
         QStringLiteral("pid")},
        {QStringLiteral("development-evidence-predecessor-pid"),
         QStringLiteral("Prior shell pid"), QStringLiteral("pid")},
        {QStringLiteral("profile"), QStringLiteral("Profile marker"),
         QStringLiteral("id")},
        {QStringLiteral("theme"), QStringLiteral("Theme marker"),
         QStringLiteral("id")},
        {QStringLiteral("quick-exit"), QStringLiteral("Exit immediately")},
        {QStringLiteral("hold"), QStringLiteral("Wait for a parent-death test")},
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
    const bool shellRole = parser.isSet(QStringLiteral("compositor-pid"));
    if (shellRole) {
        const qint64 processId =
            parser.value(QStringLiteral("compositor-pid")).toLongLong(&valid);
        if (!valid || processId <= 1) {
            return 5;
        }
    }
    QTextStream(stdout) << "token-channel-ok "
                        << (shellRole ? "shell" : "host") << '\n';
    // The production supervisor always passes --profile to the shell helper,
    // so this test-only marker lets process tests keep both generations alive
    // without adding a recovery knob to the production API.
    const bool hold = parser.isSet(QStringLiteral("hold"))
        || parser.value(QStringLiteral("profile"))
               == QLatin1String("test-hold-shell");
    if (shellRole
        && qEnvironmentVariable("QINDAQT_TEST_SESSION1_LOGOUT")
               == QLatin1String("1")) {
        QTimer::singleShot(150, &application, [&application] {
            QDBusInterface session(QStringLiteral("org.qindaqt.Session1"),
                                   QStringLiteral("/org/qindaqt/Session1"),
                                   QStringLiteral("org.qindaqt.Session1"),
                                   QDBusConnection::sessionBus());
            const QDBusReply<bool> canLogout = session.call(QStringLiteral("CanLogout"));
            if (!canLogout.isValid() || !canLogout.value()) {
                application.exit(8);
                return;
            }
            session.asyncCall(QStringLiteral("Logout"));
        });
    }
    const int lifetime = hold
        ? 120'000
        : (parser.isSet(QStringLiteral("quick-exit")) || shellRole ? 20 : 120'000);
    QTimer::singleShot(lifetime, &application, &QCoreApplication::quit);
    return application.exec();
}
