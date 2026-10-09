// SPDX-License-Identifier: GPL-3.0-or-later
#include <wp/wp.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <sys/stat.h>
#include <unistd.h>

// Fixture subprocess only. No graph is constructed, and WpState is allowed to
// write only below the authenticated per-test state root. Never invoke it in a
// normal login environment or treat it as a product configuration command.
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().size() != 2 || app.arguments().at(1) != QLatin1String("--seed-private-state"))
        return 64;
    const QString root = qEnvironmentVariable("XDG_STATE_HOME");
    const QByteArray encoded = root.toUtf8();
    struct stat metadata{};
    if (!root.startsWith(QLatin1String("/tmp/qindaqt-console-gain-"))
        || !root.endsWith(QLatin1String("/state")) || QFileInfo(root).canonicalFilePath() != root
        || ::lstat(encoded.constData(), &metadata) < 0 || !S_ISDIR(metadata.st_mode)
        || metadata.st_uid != ::geteuid() || (metadata.st_mode & 0077) != 0
        || !qgetenv("DBUS_SESSION_BUS_ADDRESS").contains("/absent-session")
        || !qgetenv("DBUS_SYSTEM_BUS_ADDRESS").contains("/absent-system"))
        return 65;
    wp_init(WP_INIT_ALL);
    WpState *state = wp_state_new("stream-properties");
    const QString location = QString::fromUtf8(wp_state_get_location(state));
    if (!location.startsWith(root + QLatin1String("/wireplumber/"))) {
        g_object_unref(state); return 66;
    }
    WpProperties *values = wp_properties_new_empty();
    wp_properties_set(values, "Output/Audio:application.id:qindaqt.gain-quiet-test",
        R"({"volume":1.0,"mute":false,"channelVolumes":[0.25,0.5],"channelMap":["FL","FR"]})");
    GError *error = nullptr;
    const bool saved = wp_state_save(state, values, &error) != FALSE;
    g_clear_error(&error); wp_properties_unref(values); g_object_unref(state);
    return saved ? 0 : 67;
}
