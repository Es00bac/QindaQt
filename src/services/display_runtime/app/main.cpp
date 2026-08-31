// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/display_journal/file_journal_store.h>
#include <qindaqt/services/display_runtime/resident_display_runtime.h>
#include <qindaqt/services/display_runtime/state_root.h>
#include <qindaqt/services/display_service/display_service_ports.h>
#include <qindaqt/services/display_writer/production_output_management_port.h>

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QUuid>
#include <QtDBus/QDBusConnection>

using namespace QindaQt;

namespace
{

class SystemMonotonicClock final : public DisplayTransaction::MonotonicClock
{
public:
    SystemMonotonicClock() { m_timer.start(); }

    quint64 nowMilliseconds() const noexcept override
    {
        return static_cast<quint64>(m_timer.elapsed());
    }

private:
    QElapsedTimer m_timer;
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-display-service"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QindaQt Display1 resident service"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption stateRootOption(
        QStringLiteral("state-root"),
        QStringLiteral("Use an explicit existing QindaQt state directory."),
        QStringLiteral("path"));
    parser.addOption(stateRootOption);
    parser.process(application);

    const DisplayRuntime::StateRootSelection stateRoot =
        DisplayRuntime::selectStateRoot(
            {.explicitPath = parser.value(stateRootOption),
             .systemdStateDirectory = qEnvironmentVariable("STATE_DIRECTORY"),
             .xdgStateHome = qEnvironmentVariable("XDG_STATE_HOME"),
             .home = qEnvironmentVariable("HOME")});
    if (!stateRoot.accepted()) {
        qCritical("Display1 state-root selection failed: %s",
                  qPrintable(stateRoot.reasonCode));
        return 2;
    }

    auto journalStore =
        std::make_unique<DisplayJournal::FileJournalStore>(stateRoot.path);
    const DisplayJournal::LoadResult loaded = journalStore->load();
    if (loaded.status == DisplayJournal::LoadStatus::Rejected) {
        // AGENT-GUARD: A rejected pathname may be the only recovery evidence.
        // Do not start the Wayland writer or clear/quarantine the file.
        qCritical("Display1 journal load rejected: %s",
                  qPrintable(loaded.reasonCode));
        return 3;
    }
    std::optional<DisplayTransaction::Journal> startupJournal;
    if (loaded.loaded()) {
        startupJournal = loaded.journal;
    }

    QDBusConnection sessionConnection = QDBusConnection::sessionBus();
    QDBusConnection systemConnection = QDBusConnection::systemBus();
    if (!sessionConnection.isConnected() || !systemConnection.isConnected()) {
        qCritical("Display1 requires connected session and system buses");
        return 4;
    }
    // AGENT-GUARD: Display1 lineage belongs to the constructing session bus.
    // A bus replacement terminates the process instead of reusing epochs.
    if (!sessionConnection.connect(
            QString{}, QStringLiteral("/org/freedesktop/DBus/Local"),
            QStringLiteral("org.freedesktop.DBus.Local"),
            QStringLiteral("Disconnected"), &application, SLOT(quit()))) {
        qCritical("Display1 could not bind constructing-bus lifetime");
        return 4;
    }

    auto writer = std::make_unique<DisplayWriter::WriterTransactionPort>(
        DisplayWriter::makeProductionOutputManagementPort(),
        std::move(journalStore));
    DisplayRuntime::ResidentDisplayRuntime runtime(
        DisplayService::makeCompositorInventorySource(sessionConnection),
        std::move(writer), DisplayRuntime::makeQtSessionSafetyPort(
                               sessionConnection, systemConnection),
        std::make_unique<SystemMonotonicClock>(),
        [] { return QUuid::createUuid().toString(QUuid::WithoutBraces); },
        sessionConnection, {}, {}, std::move(startupJournal));
    QObject::connect(&runtime, &DisplayRuntime::ResidentDisplayRuntime::fatalError,
                     &application, [&application](const QString &reasonCode) {
                         qCritical("Display1 runtime authority failed: %s",
                                   qPrintable(reasonCode));
                         application.exit(1);
                     });
    const DisplayRuntime::RuntimeStartStatus status = runtime.start();
    if (status != DisplayRuntime::RuntimeStartStatus::Started) {
        qCritical("Display1 runtime startup failed with status %u",
                  static_cast<unsigned int>(status));
        return 5;
    }
    QObject::connect(&application, &QCoreApplication::aboutToQuit, &runtime,
                     &DisplayRuntime::ResidentDisplayRuntime::stop);
    return QCoreApplication::exec();
}
