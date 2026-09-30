// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/keyring/legacy_reader.h>
#include <qindaqt/services/keyring/legacy_snapshot.h>
#include <qindaqt/platform/compositor_attachment/compositor_attachment.h>
#include <qindaqt/services/session_lock_state/native_lock_state_monitor.h>
#include <qindaqt/services/session_lock_state/qt_native_lock_transport.h>
#include "../daemon/session_display_binding.h"
#include "../daemon/process_prompt_provider.h"
#include "../import/import_passwords_p.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QTextStream>
#include <QThread>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <csignal>
#include <unistd.h>
using namespace qindaqt::keyring;
namespace {
volatile sig_atomic_t stopped=0;
void stop(int){stopped=1;}
int fail(CollectionImportError error) {QTextStream(stderr)<<"Import refused (code "<<static_cast<int>(error)<<"). No success is claimed.\n";return 3;}
}
int main(int argc,char **argv) {
    rlimit cores{0,0};if(setrlimit(RLIMIT_CORE,&cores)!=0 || prctl(PR_SET_DUMPABLE,0)!=0) return 2;
    umask(0077);std::signal(SIGINT,stop);std::signal(SIGTERM,stop);std::signal(SIGPIPE,SIG_IGN);
    QCoreApplication app(argc,argv);app.setApplicationName("qindaqt-keyring-import");
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &){});
    QCommandLineParser parser;parser.addHelpOption();
    parser.setApplicationDescription("One-time owner-bound legacy acquisition; never switches service names or edits source files.");
    parser.addOptions({{"bus-address","Explicit selected session bus.","address"},
        {"session-owner","Independently accepted session unique owner.","owner"},{"runtime-root","Private ordinary display directory.","path"},
        {"display","Selected ordinary socket basename.","name"},{"compositor-owner","Independently accepted compositor unique owner.","owner"},
        {"compositor-pid","Independently accepted compositor PID.","pid"},{"storage-root","Native destination (exclusive writer required).","path"},
        {"secret-owner","Accepted running Secret Service unique owner.","owner"},{"secret-pid","Accepted running Secret Service PID.","pid"},
        {"kwallet-owner","Accepted running KWallet unique owner.","owner"},{"kwallet-pid","Accepted running KWallet PID.","pid"},
        {"kwallet-service","KWallet public service (org.kde.kwalletd6 or5).","service","org.kde.kwalletd6"},
        {"alias","Additional known Secret Service alias (repeatable; default always included).","name"},
        {"password-fd","Transferred anonymous pipe/unix stream with u32-BE length + password frames; otherwise native helper.","fd"}});
    parser.process(app);
    for(const auto &name:{"bus-address","session-owner","runtime-root","display","compositor-owner","compositor-pid","storage-root"})
        if(!parser.isSet(name)) return fail(CollectionImportError::InvalidInput);
    if(!parser.isSet("secret-owner") && !parser.isSet("kwallet-owner")) return fail(CollectionImportError::InvalidInput);
    try {
        auto bus=QDBusConnection::connectToBus(parser.value("bus-address"),"qindaqt-keyring-import");
        if(!bus.isConnected()) return fail(CollectionImportError::Unavailable);
        namespace pc=QindaQt::Platform::Compositor;namespace sl=QindaQt::Services::SessionLockState;
        const auto sessionOwner=parser.value("session-owner");
        pc::CompositorAttachment attachment(bus,parser.value("runtime-root"),[&](const QString &owner){return !stopped && owner==sessionOwner;});
        bool valid=false;const auto compositorPid=parser.value("compositor-pid").toULongLong(&valid);
        if(!valid || !attachment.attach(sessionOwner,parser.value("display"),pc::PeerExpectation{parser.value("compositor-owner"),compositorPid}))
            return fail(CollectionImportError::OwnerLost);
        sl::QtNativeLockTransport lockTransport(bus);
        sl::NativeLockStateMonitor lock(lockTransport,[&](const QString &owner,quint64 pid){
            const auto identity=attachment.identity();return !stopped && identity && owner==identity->compositorOwner && pid==identity->compositorPid;
        });
        if(!lock.start()) return fail(CollectionImportError::Unavailable);
        QElapsedTimer admissionWait;admissionWait.start();
        while(lock.state()==sl::LockState::Unknown && admissionWait.elapsed()<3000 && attachment.live() && !stopped) {
            QCoreApplication::processEvents();QThread::msleep(5);
        }
        std::function<bool()> nativeAdmitted=[&]{return !stopped && attachment.live() && lock.state()==sl::LockState::Unlocked;};
        if(!nativeAdmitted()) return fail(CollectionImportError::OwnerLost);
        std::vector<std::unique_ptr<LegacyReader>> readers;
        std::vector<LegacySnapshot> snapshots;
        for(const auto &kind:{QString("secret"),QString("kwallet")}) if(parser.isSet(kind+"-owner")) {
            bool pidValid=false;const auto pid=parser.value(kind+"-pid").toULongLong(&pidValid);
            if(!pidValid) return fail(CollectionImportError::InvalidInput);
            LegacySourceBinding binding{kind=="secret"?"secret-service":"kwallet",
                kind=="secret"?"org.freedesktop.secrets":parser.value("kwallet-service"),parser.value(kind+"-owner"),sessionOwner,pid};
            auto reader=openLegacyReader(parser.value("bus-address"),std::move(binding),nativeAdmitted);
            QStringList aliases{"default"};for(const auto &alias:parser.values("alias")) if(!aliases.contains(alias)) aliases.append(alias);
            auto receipt=reader->acquire(aliases);
            if(receipt.error!=CollectionImportError::None) return fail(receipt.error);
            snapshots.push_back(std::move(receipt.snapshot));readers.push_back(std::move(reader));
        }
        auto plan=planLegacyCollections(std::move(snapshots));if(plan.error!=CollectionImportError::None) return fail(plan.error);
        std::function<bool()> admitted=[&]{
            if(!nativeAdmitted()) return false;
            for(auto &reader:readers) if(!reader->live()) return false;
            return true;
        };
        service::SessionDisplayBinding display(bus,parser.value("runtime-root"),true);
        if(!display.attach(sessionOwner,parser.value("display")) || display.compositorOwner()!=parser.value("compositor-owner")
            || display.compositorPid()!=compositorPid) return fail(CollectionImportError::OwnerLost);
        service::ProcessPromptProvider prompt(QString::fromUtf8(QINDAQT_KEYRING_PROMPT_PATH),nullptr,&display);
        int fd=-1;
        if(parser.isSet("password-fd")) {bool fdValid=false;fd=parser.value("password-fd").toInt(&fdValid);if(!fdValid || fd<0) return fail(CollectionImportError::InvalidInput);}
        importer::Passwords passwords(fd,prompt,admitted);
        auto catalog=openCollectionImportCatalog(parser.value("storage-root"));
        const auto receipt=catalog->commit(std::move(plan.batch),passwords,admitted,[]{QCoreApplication::processEvents();});
        QCoreApplication::processEvents();
        if(receipt.error!=CollectionImportError::None) return fail(receipt.error);
        if(!admitted()) return fail(CollectionImportError::OwnerLost);
        QTextStream(stdout)<<"Import sealed: "<<receipt.collectionsAdded<<" collections, "<<receipt.itemsAdded<<" items; "
            <<receipt.collectionsUnchanged<<" collections unchanged. Source files retained; service names unchanged.\n";
        return 0;
    } catch(...) {return fail(CollectionImportError::Unavailable);}
}
