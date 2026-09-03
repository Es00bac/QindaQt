// SPDX-License-Identifier: GPL-3.0-or-later

#include "fake_dbusmenu_exporter.h"

#include <qindaqt/shell/global_menu/registrar/appmenu_registrar.h>

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QTextStream>

using namespace QindaQt::Shell::GlobalMenu;

namespace {

constexpr auto kProviderService = "org.qindaqt.TestHostileGlobalMenu";

class ProviderProbe final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.TestHostileGlobalMenuProbe1")

public:
    explicit ProviderProbe(const Test::FakeDbusMenuExporter &exporter,
                           QObject *parent = nullptr)
        : QObject(parent)
        , m_exporter(exporter)
    {
    }

public Q_SLOTS:
    Q_SCRIPTABLE int EventCount() const { return m_exporter.eventCount(); }

private:
    const Test::FakeDbusMenuExporter &m_exporter;
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    DbusMenu::registerDbusMenuWireTypes();
    Registrar::registerRegistrarWireTypes();
    auto bus = QDBusConnection::connectToBus(
        QDBusConnection::SessionBus, QStringLiteral("hostile-global-menu-provider"));
    if (!bus.isConnected()) {
        return 2;
    }

    Test::FakeDbusMenuExporter exporter;
    exporter.setLayout(1, Test::menuLayout(QStringLiteral("_Hostile")));
    ProviderProbe probe(exporter);
    const auto exportFlags = QDBusConnection::ExportScriptableSlots
        | QDBusConnection::ExportScriptableSignals
        | QDBusConnection::ExportScriptableProperties;
    if (!bus.registerObject(QStringLiteral("/Menu"), &exporter, exportFlags)
        || !bus.registerObject(QStringLiteral("/Probe"), &probe, exportFlags)
        || !bus.registerService(QString::fromLatin1(kProviderService))) {
        return 3;
    }

    QDBusMessage registration = QDBusMessage::createMethodCall(
        QString::fromLatin1(Registrar::kRegistrarServiceName),
        QString::fromLatin1(Registrar::kRegistrarObjectPath),
        QString::fromLatin1(Registrar::kRegistrarInterface),
        QStringLiteral("RegisterWindow"));
    registration.setArguments(
        {QVariant::fromValue(quint32{77}),
         QVariant::fromValue(QDBusObjectPath(QStringLiteral("/Menu")))});
    if (bus.call(registration, QDBus::Block, 5'000).type()
        != QDBusMessage::ReplyMessage) {
        return 4;
    }

    QTextStream stream(stdout);
    stream << "READY\n";
    stream.flush();
    return application.exec();
}

#include "hostile_menu_provider.moc"
