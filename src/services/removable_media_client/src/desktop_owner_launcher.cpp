// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/removable_media_client/desktop_owner_launcher.h>
#include <qindaqt/application_catalog/application_directory_scan.h>
#include <qindaqt/application_catalog/launch_support.h>
#include <QProcess>
#include <utility>
namespace QindaQt::RemovableMedia {
namespace {
class ProcessStarter final : public MediaArgvStarter {
public:
    bool start(const QString &program, const QStringList &arguments) override {
        return QProcess::startDetached(program, arguments);
    }
};
}
DesktopMediaOwnerLauncher::DesktopMediaOwnerLauncher(QStringList dataRoots)
    : m_dataRoots(std::move(dataRoots)), m_ownedStarter(std::make_unique<ProcessStarter>()),
      m_starter(m_ownedStarter.get()) {}
DesktopMediaOwnerLauncher::DesktopMediaOwnerLauncher(QStringList dataRoots, MediaArgvStarter &starter)
    : m_dataRoots(std::move(dataRoots)), m_starter(&starter) {}
DesktopMediaOwnerLauncher::~DesktopMediaOwnerLauncher() = default;
bool DesktopMediaOwnerLauncher::startOwner() {
    const auto scan = QindaQt::ApplicationCatalog::scanApplicationDirectories(m_dataRoots);
    auto ownerId = QString::fromLatin1(kOwnerDesktopId);
    // AGENT-CONTRACT: ApplicationCatalog ids omit the .desktop suffix, while
    // the protocol names the installed filename. Both designate one fixed id.
    ownerId.chop(8);
    const auto *application = scan.application(ownerId);
    if (!application) return false;
    const auto plan = QindaQt::ApplicationCatalog::planApplicationLaunch(application->documentText,
        {}, application->entry.name, application->desktopFilePath);
    if (plan.support != QindaQt::ApplicationCatalog::LaunchSupport::ProcessSpawn) return false;
    // No terminal or mount utility fallback; observed inventory proves ready.
    return m_starter->start(plan.program, plan.arguments);
}
}
