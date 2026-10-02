// SPDX-License-Identifier: LGPL-3.0-or-later
#include "print_adaptor.h"
#include "print_policy.h"
#include <QDateTime>
#include <QRandomGenerator>
#include <sys/stat.h>
#include <fcntl.h>
namespace QindaQt::Services::Portal {
PrintAdaptor::PrintAdaptor(QObject &host, RequestRegistry &requests, MiscUi &ui)
    : QDBusAbstractAdaptor(&host), m_requests(requests), m_ui(ui) {
    connect(&ui, &MiscUi::completed, this, [this](RequestToken token, RequestResponse response, const QJsonObject &output) {
        const auto it = m_pending.constFind(token); if (it == m_pending.cend()) return;
        const auto pending = *it; QVariantMap results;
        if (!m_ui.admitted() || !m_requests.live(token) || pending.owner != m_requests.frontendOwner()) response = RequestResponse::Failed;
        if (response == RequestResponse::Success && pending.prepare) {
            if (!validPrintConfiguration(output)) response = RequestResponse::Failed;
            else {
                const auto now = QDateTime::currentMSecsSinceEpoch();
                for (auto i = m_prepared.begin(); i != m_prepared.end();) {
                    if (i->expiry < now || i->owner != pending.owner) i = m_prepared.erase(i); else ++i;
                }
                if (m_prepared.size() >= 32) response = RequestResponse::Failed;
                else {
                    quint32 ticket = 0; do { ticket = QRandomGenerator::system()->generate(); } while (!ticket || m_prepared.contains(ticket));
                    m_prepared.insert(ticket, {pending.app, pending.owner, output, now + 300000});
                    results = {{"settings", output.value("settings").toObject().toVariantMap()}, {"page-setup", output.value("page-setup").toObject().toVariantMap()}, {"token", ticket}};
                }
            }
        } else if (response == RequestResponse::Success && !output.isEmpty()) response = RequestResponse::Failed;
        m_requests.finish(token, response, response == RequestResponse::Success ? results : QVariantMap{});
    });
    connect(&ui, &MiscUi::authorityLost, this, [this] { m_prepared.clear(); for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); });
}
PrintAdaptor::~PrintAdaptor() { for (const auto token : m_pending.keys()) m_requests.retire(token, RequestResponse::Failed); }
quint32 PrintAdaptor::begin(bool prepare, const QDBusObjectPath &handle, const QString &app, const QString &parent,
    const QString &title, const QVariantMap &settings, const QVariantMap &pages, const QVariantMap &options,
    int fd, const QDBusMessage &call, QVariantMap &results) {
    results.clear(); const auto slot = std::make_shared<RequestToken>(0);
    const auto token = m_requests.begin(call, handle.path(), app, [this, slot](RequestResponse) { m_pending.remove(*slot); m_ui.cancel(*slot); });
    *slot = token; if (!token) return 2;
    auto frame = miscFrame(prepare ? "prepare-print" : "print", app, parent, title, options);
    if (!frame || !m_ui.admitted() || !validPrintMaps(settings, pages)) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    if (options.contains("accept_label") && (options.value("accept_label").metaType() != QMetaType::fromType<QString>() || !boundedText(options.value("accept_label").toString(), 256))) { m_requests.finish(token, RequestResponse::Failed); return 2; }
    frame->insert("accept_label", options.value("accept_label").toString());
    frame->insert("settings", QJsonObject::fromVariantMap(settings)); frame->insert("page-setup", QJsonObject::fromVariantMap(pages));
    if (!prepare) {
        struct stat status{}; const int flags = fcntl(fd, F_GETFL);
        if (fd < 0 || flags < 0 || (flags & O_ACCMODE) == O_WRONLY || fstat(fd, &status) || !S_ISREG(status.st_mode) || status.st_size > 536870912) { m_requests.finish(token, RequestResponse::Failed); return 2; }
        if (options.contains("token")) {
            const auto value = options.value("token"); const auto ticket = value.toUInt(); const auto found = m_prepared.find(ticket);
            if (value.metaType() != QMetaType::fromType<quint32>() || found == m_prepared.end() || found->app != app || found->owner != call.service() || found->expiry < QDateTime::currentMSecsSinceEpoch()) { m_requests.finish(token, RequestResponse::Failed); return 2; }
            frame->insert("configuration", found->configuration); m_prepared.erase(found);
        }
    }
    m_pending.insert(token, {prepare, app, call.service()}); m_ui.present(token, *frame, fd); return 2;
}
quint32 PrintAdaptor::PreparePrint(const QDBusObjectPath &h, const QString &a, const QString &p, const QString &t,
    const QVariantMap &s, const QVariantMap &pages, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(true, h, a, p, t, s, pages, o, -1, c, r); }
quint32 PrintAdaptor::Print(const QDBusObjectPath &h, const QString &a, const QString &p, const QString &t,
    const QDBusUnixFileDescriptor &fd, const QVariantMap &o, const QDBusMessage &c, QVariantMap &r) { return begin(false, h, a, p, t, {}, {}, o, fd.fileDescriptor(), c, r); }
}
