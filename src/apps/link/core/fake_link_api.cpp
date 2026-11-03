// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_link_api.h"

#include <QtCore/QBuffer>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QTimer>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtGui/QPainter>

#include <algorithm>

namespace QindaQt::LinkApp {
namespace {

QString compact(const QJsonValue &value)
{
    if (value.isArray()) return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
}

QByteArray pngBase64(int width, int height, const QColor &top, const QColor &bottom)
{
    QImage image(width, height, QImage::Format_RGB32);
    QPainter painter(&image);
    QLinearGradient gradient(0, 0, 0, height);
    gradient.setColorAt(0, top);
    gradient.setColorAt(1, bottom);
    painter.fillRect(image.rect(), gradient);
    painter.end();
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes.toBase64();
}

QJsonObject makeThread(qint64 id, const QStringList &addrs, const QStringList &names, const QString &snippet,
                   qint64 date, int unread, int count)
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("addrs"), QJsonArray::fromStringList(addrs)},
                       {QStringLiteral("names"), QJsonArray::fromStringList(names)},
                       {QStringLiteral("snippet"), snippet},
                       {QStringLiteral("date"), date},
                       {QStringLiteral("unread"), unread},
                       {QStringLiteral("count"), count}};
}

QJsonObject makeMessage(qint64 id, const QString &box, const QString &body, qint64 date, const QString &kind = {},
                    const QJsonArray &parts = {})
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("kind"), kind.isEmpty() ? QStringLiteral("sms") : kind},
                       {QStringLiteral("box"), box},
                       {QStringLiteral("addr"), QStringLiteral("+15550100")},
                       {QStringLiteral("date"), date},
                       {QStringLiteral("body"), body},
                       {QStringLiteral("trunc"), false},
                       {QStringLiteral("read"), true},
                       {QStringLiteral("parts"), parts}};
}

// Provider order, newest first: (date desc, kind mms before sms, id desc).
bool newerFirst(const QJsonObject &a, const QJsonObject &b)
{
    const qint64 da = qint64(a.value(QStringLiteral("date")).toDouble());
    const qint64 db = qint64(b.value(QStringLiteral("date")).toDouble());
    if (da != db) return da > db;
    const bool mmsA = a.value(QStringLiteral("kind")).toString() == QLatin1String("mms");
    const bool mmsB = b.value(QStringLiteral("kind")).toString() == QLatin1String("mms");
    if (mmsA != mmsB) return mmsA;
    return a.value(QStringLiteral("id")).toDouble() > b.value(QStringLiteral("id")).toDouble();
}

bool strictlyOlder(const QJsonObject &row, qint64 before, const QString &kind, qint64 beforeId)
{
    const qint64 date = qint64(row.value(QStringLiteral("date")).toDouble());
    if (date != before) return date < before;
    const bool mms = row.value(QStringLiteral("kind")).toString() == QLatin1String("mms");
    const bool cursorMms = kind == QLatin1String("mms");
    if (mms != cursorMms) return cursorMms;  // an sms is older than an mms at the same instant
    return qint64(row.value(QStringLiteral("id")).toDouble()) < beforeId;
}

} // namespace

FakeLinkApi::FakeLinkApi(Scenario scenario, QObject *parent) : LinkApi(parent)
{
    if (scenario == Scenario::Unavailable) m_available = false;
    if (scenario == Scenario::Error) errorForAll = QStringLiteral("internal");
    if (scenario == Scenario::Populated) populate();
    if (scenario == Scenario::Empty) devicesJson = QStringLiteral("[]");
}

FakeLinkApi::Scenario FakeLinkApi::scenarioFromName(const QString &name, bool *ok)
{
    if (ok != nullptr) *ok = true;
    if (name == QLatin1String("populated")) return Scenario::Populated;
    if (name == QLatin1String("empty")) return Scenario::Empty;
    if (name == QLatin1String("unavailable")) return Scenario::Unavailable;
    if (name == QLatin1String("error")) return Scenario::Error;
    if (ok != nullptr) *ok = false;
    return Scenario::Populated;
}

void FakeLinkApi::setAvailable(bool available)
{
    if (available == m_available) return;
    m_available = available;
    Q_EMIT availableChanged();
}

void FakeLinkApi::populate()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 minute = 60 * 1000;
    devicesJson = compact(QJsonArray{QJsonObject{
        {QStringLiteral("id"), QString(64, QLatin1Char('a'))},
        {QStringLiteral("name"), QStringLiteral("Demo phone")},
        {QStringLiteral("model"), QStringLiteral("SM-DEMO1")},
        {QStringLiteral("paired"), true},
        {QStringLiteral("connected"), true},
        {QStringLiteral("ready"), true},
        {QStringLiteral("transport"), QStringLiteral("usb")},
        {QStringLiteral("battery"), QJsonObject{{QStringLiteral("level"), 87}, {QStringLiteral("charging"), true},
                                                {QStringLiteral("plugged"), QStringLiteral("usb")}}},
        {QStringLiteral("network"), QJsonObject{{QStringLiteral("type"), QStringLiteral("wifi")},
                                                {QStringLiteral("signal"), 3}}},
        {QStringLiteral("lastSeen"), now},
        {QStringLiteral("caps"), QJsonArray{QStringLiteral("notify"), QStringLiteral("sms"), QStringLiteral("apps"),
                                            QStringLiteral("ring")}}}});
    adb = QJsonArray{QJsonObject{{QStringLiteral("serial"), QStringLiteral("DEMO0001")},
                                 {QStringLiteral("appInstalled"), true}, {QStringLiteral("tunneled"), true}}};
    const QStringList firstNames{QStringLiteral("Sam Carter"), QStringLiteral("Mina Okafor"), QStringLiteral("Dev Patel"),
                                 QStringLiteral("Lena Fischer"), QStringLiteral("Ravi Menon"), QStringLiteral("Ola Berg")};
    const QStringList snippets{QStringLiteral("See you at 6, I'll bring the charger"),
                               QStringLiteral("Thanks, that works for me"),
                               QStringLiteral("Can you send the photo again?"),
                               QStringLiteral("Running ten minutes late"),
                               QStringLiteral("Your code is 123456"),
                               QStringLiteral("Lunch tomorrow?")};
    for (int i = 0; i < 24; ++i) {
        const bool known = i < firstNames.size();
        threads.append(makeThread(i + 1, {QStringLiteral("+1555010%1").arg(i, 2, 10, QLatin1Char('0'))},
                              {known ? firstNames.at(i) : QString()}, snippets.at(i % snippets.size()),
                              now - (i * 37 + 3) * minute, i == 0 ? 2 : i == 3 ? 1 : 0, 3 + i * 5));
    }
    threads[4] = makeThread(5, {QStringLiteral("22395")}, {QString()}, QStringLiteral("Your code is 123456"),
                        now - 150 * minute, 0, 1);
    // Thread 1 has 130 messages (paging), including one MMS with an image.
    QList<QJsonObject> list;
    for (int i = 0; i < 130; ++i) {
        const bool mine = i % 3 == 1;
        const qint64 date = now - (130 - i) * 7 * minute;
        list.append(makeMessage(1000 + i, mine ? QStringLiteral("sent") : QStringLiteral("inbox"),
                            mine ? QStringLiteral("On my way, message %1").arg(i)
                                 : QStringLiteral("Sounds good, message %1").arg(i),
                            date));
    }
    list.append(makeMessage(40, QStringLiteral("inbox"), QStringLiteral("Here is the photo"), now - 20 * minute,
                        QStringLiteral("mms"),
                        QJsonArray{QJsonObject{{QStringLiteral("id"), 900}, {QStringLiteral("mime"), QStringLiteral("image/png")},
                                               {QStringLiteral("name"), QStringLiteral("photo.png")},
                                               {QStringLiteral("size"), 4000}}}));
    list.append(makeMessage(1999, QStringLiteral("inbox"), QStringLiteral("See you at 6, I'll bring the charger"),
                        now - 3 * minute));
    std::sort(list.begin(), list.end(), newerFirst);
    messages.insert(1, list);
    for (int t = 2; t <= 6; ++t) {
        messages.insert(t, {makeMessage(t * 100, QStringLiteral("inbox"), threads.at(t - 1).value(QStringLiteral("snippet")).toString(),
                                    qint64(threads.at(t - 1).value(QStringLiteral("date")).toDouble()))});
    }
    apps = QJsonArray{
        QJsonObject{{QStringLiteral("pkg"), QStringLiteral("com.example.chat")}, {QStringLiteral("label"), QStringLiteral("Chat")}, {QStringLiteral("muted"), false}},
        QJsonObject{{QStringLiteral("pkg"), QStringLiteral("com.example.mail")}, {QStringLiteral("label"), QStringLiteral("Mail")}, {QStringLiteral("muted"), false}},
        QJsonObject{{QStringLiteral("pkg"), QStringLiteral("com.example.news")}, {QStringLiteral("label"), QStringLiteral("News")}, {QStringLiteral("muted"), true}},
        QJsonObject{{QStringLiteral("pkg"), QStringLiteral("com.example.shop")}, {QStringLiteral("label"), QStringLiteral("Shopping")}, {QStringLiteral("muted"), false}},
        QJsonObject{{QStringLiteral("pkg"), QStringLiteral("com.example.game")}, {QStringLiteral("label"), QStringLiteral("Puzzle Game")}, {QStringLiteral("muted"), true}}};
    currentJson = QJsonArray{QJsonObject{{QStringLiteral("device"), QString(64, QLatin1Char('a'))},
                                         {QStringLiteral("key"), QStringLiteral("0|com.example.chat|7|null|1")},
                                         {QStringLiteral("app"), QStringLiteral("Chat")}}};
    pending = QJsonArray{};
    setupReportJson = compact(QJsonObject{});
}

QByteArray FakeLinkApi::inventoryJson()
{
    struct App { const char *pkg; const char *label; QColor top; QColor bottom; };
    const App apps[] = {{"com.example.chat", "Chat", QColor("#4f8cff"), QColor("#1d4fd8")},
                        {"com.example.mail", "Mail", QColor("#ff7a59"), QColor("#c2410c")},
                        {"com.example.maps", "Maps", QColor("#34d399"), QColor("#047857")},
                        {"com.example.camera", "Camera", QColor("#a78bfa"), QColor("#6d28d9")},
                        {"com.example.music", "Music", QColor("#f472b6"), QColor("#be185d")},
                        {"com.example.notes", "Notes", QColor("#fbbf24"), QColor("#b45309")},
                        {"com.example.bank", "Bank", QColor("#38bdf8"), QColor("#0369a1")},
                        {"com.example.clock", "Clock", QColor("#94a3b8"), QColor("#334155")}};
    QJsonArray out;
    for (const App &a : apps) {
        out.append(QJsonObject{{QStringLiteral("package"), QLatin1String(a.pkg)},
                               {QStringLiteral("label"), QLatin1String(a.label)},
                               {QStringLiteral("icon"), QString::fromLatin1(pngBase64(96, 96, a.top, a.bottom))}});
    }
    return QJsonDocument(QJsonObject{{QStringLiteral("apps"), out}}).toJson(QJsonDocument::Compact);
}

void FakeLinkApi::later(std::function<void()> work) { QTimer::singleShot(0, this, std::move(work)); }

bool FakeLinkApi::fails(const Reply &done)
{
    if (!m_available) {
        later([done] { if (done) done({}, QStringLiteral("unavailable")); });
        return true;
    }
    if (!errorForAll.isEmpty()) {
        later([done, code = errorForAll] { if (done) done({}, code); });
        return true;
    }
    return false;
}

void FakeLinkApi::devices(Reply done)
{
    calls << QStringLiteral("devices");
    if (fails(done)) return;
    later([done, json = devicesJson.isEmpty() ? QStringLiteral("[]") : devicesJson] { if (done) done(json, {}); });
}

void FakeLinkApi::pendingPairings(Reply done)
{
    calls << QStringLiteral("pendingPairings");
    if (!pendingSupported) {
        later([done] { if (done) done({}, QStringLiteral("unsupported")); });
        return;
    }
    if (fails(done)) return;
    later([done, json = compact(pending)] { if (done) done(json, {}); });
}

void FakeLinkApi::confirmPairing(const QString &id, bool accept)
{
    calls << QStringLiteral("confirmPairing");
    confirmations.insert(id, accept);
}

void FakeLinkApi::unpair(const QString &id)
{
    calls << QStringLiteral("unpair");
    unpaired << id;
}

void FakeLinkApi::listThreads(const QString &, int limit, qint64 before, qint64 beforeId, Reply done)
{
    calls << QStringLiteral("listThreads");
    if (fails(done)) return;
    limit = std::clamp(limit, 1, 100);
    QJsonArray page;
    bool more = false;
    int unread = 0;
    for (const QJsonObject &t : std::as_const(threads)) unread += t.value(QStringLiteral("unread")).toInt();
    for (const QJsonObject &t : std::as_const(threads)) {
        const qint64 date = qint64(t.value(QStringLiteral("date")).toDouble());
        const qint64 id = qint64(t.value(QStringLiteral("id")).toDouble());
        if (before > 0 && !(date < before || (date == before && id < beforeId))) continue;
        if (page.size() == limit) { more = true; break; }
        page.append(t);
    }
    later([done, json = compact(QJsonObject{{QStringLiteral("threads"), page}, {QStringLiteral("more"), more},
                                            {QStringLiteral("unread"), unread}})] { if (done) done(json, {}); });
}

void FakeLinkApi::listMessages(const QString &, qint64 thread, int limit, qint64 before, const QString &beforeKind,
                               qint64 beforeId, Reply done)
{
    calls << QStringLiteral("listMessages");
    if (fails(done)) return;
    limit = std::clamp(limit, 1, 200);
    QJsonArray page;
    bool more = false;
    for (const QJsonObject &m : messages.value(thread)) {
        if (before > 0 && !strictlyOlder(m, before, beforeKind.isEmpty() ? QStringLiteral("sms") : beforeKind, beforeId)) continue;
        if (page.size() == limit) { more = true; break; }
        page.append(m);
    }
    later([done, json = compact(QJsonObject{{QStringLiteral("thread"), thread}, {QStringLiteral("messages"), page},
                                            {QStringLiteral("more"), more}})] { if (done) done(json, {}); });
}

void FakeLinkApi::getPart(const QString &, qint64 part, int, Reply done)
{
    calls << QStringLiteral("getPart");
    if (fails(done)) return;
    later([done, part, data = QString::fromLatin1(pngBase64(480, 320, QColor("#f59e0b"), QColor("#7c2d12")))] {
        if (done) {
            done(compact(QJsonObject{{QStringLiteral("part"), part}, {QStringLiteral("mime"), QStringLiteral("image/png")},
                                     {QStringLiteral("size"), 4000}, {QStringLiteral("scaled"), false},
                                     {QStringLiteral("data"), data}}), {});
        }
    });
}

void FakeLinkApi::send(const QString &, const QStringList &addrs, const QString &text, Reply done)
{
    calls << QStringLiteral("send");
    sentTo.append(addrs);
    sentTexts.append(text);
    if (!sendError.isEmpty()) {
        later([done, code = sendError] { if (done) done({}, code); });
        return;
    }
    if (fails(done)) return;
    const qint64 ref = nextRef++;
    later([done, ref] { if (done) done(compact(QJsonObject{{QStringLiteral("ref"), ref}}), {}); });
}

void FakeLinkApi::markRead(const QString &, qint64 thread, Reply done)
{
    calls << QStringLiteral("markRead");
    if (fails(done)) return;
    later([done, thread] { if (done) done(compact(QJsonObject{{QStringLiteral("thread"), thread}, {QStringLiteral("unread"), 0}}), {}); });
}

void FakeLinkApi::listApps(Reply done)
{
    calls << QStringLiteral("listApps");
    if (fails(done)) return;
    QJsonArray out;
    for (const QJsonValue &v : std::as_const(apps)) {
        QJsonObject o = v.toObject();
        if (muted.contains(o.value(QStringLiteral("pkg")).toString())) o.insert(QStringLiteral("muted"), muted.value(o.value(QStringLiteral("pkg")).toString()));
        out.append(o);
    }
    later([done, json = compact(out)] { if (done) done(json, {}); });
}

void FakeLinkApi::setAppMuted(const QString &pkg, bool isMuted)
{
    calls << QStringLiteral("setAppMuted");
    muted.insert(pkg, isMuted);
}

void FakeLinkApi::currentNotifications(Reply done)
{
    calls << QStringLiteral("currentNotifications");
    if (fails(done)) return;
    later([done, json = compact(currentJson)] { if (done) done(json, {}); });
}

void FakeLinkApi::reply(const QString &device, const QString &key, const QString &action, const QString &, Reply done)
{
    calls << QStringLiteral("reply");
    replies << device + QLatin1Char('|') + key + QLatin1Char('|') + action;
    if (fails(done)) return;
    later([done] { if (done) done({}, {}); });
}

void FakeLinkApi::adbDevices(Reply done)
{
    calls << QStringLiteral("adbDevices");
    if (fails(done)) return;
    later([done, json = compact(adb)] { if (done) done(json, {}); });
}

void FakeLinkApi::setupPhone(const QString &, Reply done)
{
    calls << QStringLiteral("setupPhone");
    if (fails(done)) return;
    later([done, json = setupReportJson] { if (done) done(json, {}); });
}

void FakeLinkApi::setupStatus(const QString &, Reply done)
{
    calls << QStringLiteral("setupStatus");
    if (fails(done)) return;
    later([done, json = statusReportJson] { if (done) done(json, {}); });
}

} // namespace QindaQt::LinkApp
