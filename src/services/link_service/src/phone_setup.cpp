// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_service/phone_setup.h>

#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>

#include <memory>

namespace QindaQt::Link {
namespace {

const QStringList &permissions()
{
    static const QStringList list = {
        QStringLiteral("READ_SMS"),           QStringLiteral("SEND_SMS"),       QStringLiteral("RECEIVE_SMS"),
        QStringLiteral("READ_CONTACTS"),      QStringLiteral("POST_NOTIFICATIONS"),
        QStringLiteral("READ_PHONE_STATE")};
    return list;
}

constexpr auto kListener = "org.qindaqt.link.android.NotificationListener";

} // namespace

PhoneSetup::PhoneSetup(AdbRunner &runner, QString package, QObject *parent)
    : QObject(parent), m_runner(runner), m_package(std::move(package))
{
}

bool PhoneSetup::validSerial(const QString &serial)
{
    static const QRegularExpression shape(QRegularExpression::anchoredPattern(QStringLiteral("[A-Za-z0-9._:-]{1,64}")));
    return shape.match(serial).hasMatch();
}

QList<PhoneSetup::Step> PhoneSetup::grantSteps() const
{
    QList<Step> steps;
    for (const QString &permission : permissions()) {
        steps.append({QStringLiteral("grant:") + permission,
                      {QStringLiteral("pm"), QStringLiteral("grant"), m_package,
                       QStringLiteral("android.permission.") + permission},
                      QString(), false});
    }
    steps.append({QStringLiteral("restricted-settings"),
                  {QStringLiteral("appops"), QStringLiteral("set"), m_package,
                   QStringLiteral("ACCESS_RESTRICTED_SETTINGS"), QStringLiteral("allow")},
                  QString(), false});
    steps.append({QStringLiteral("notification-listener"),
                  {QStringLiteral("cmd"), QStringLiteral("notification"), QStringLiteral("allow_listener"),
                   m_package + QLatin1Char('/') + QLatin1String(kListener)},
                  QString(), false});
    steps.append({QStringLiteral("battery-whitelist"),
                  {QStringLiteral("cmd"), QStringLiteral("deviceidle"), QStringLiteral("whitelist"),
                   QLatin1Char('+') + m_package},
                  QString(), false});
    return steps;
}

void PhoneSetup::run(const QString &serial, Done done)
{
    if (m_running.contains(serial)) {
        done(QJsonObject{{QStringLiteral("serial"), serial},
                         {QStringLiteral("ok"), false},
                         {QStringLiteral("error"), QStringLiteral("busy")},
                         {QStringLiteral("steps"), QJsonArray()}});
        return;
    }
    const bool track = validSerial(serial);
    if (track) m_running.insert(serial);
    runSteps(serial, grantSteps(), false, [this, serial, track, done = std::move(done)](const QJsonObject &report) {
        if (track) m_running.remove(serial);
        done(report);
    });
}

void PhoneSetup::status(const QString &serial, Done done)
{
    // One dumpsys answers every permission; the other three each have a cheap read.
    QList<Step> steps;
    steps.append({QStringLiteral("package"), {QStringLiteral("dumpsys"), QStringLiteral("package"), m_package},
                  QString(), false});
    steps.append({QStringLiteral("restricted-settings"),
                  {QStringLiteral("appops"), QStringLiteral("get"), m_package,
                   QStringLiteral("ACCESS_RESTRICTED_SETTINGS")},
                  QStringLiteral(": allow"), false});
    steps.append({QStringLiteral("notification-listener"),
                  {QStringLiteral("settings"), QStringLiteral("get"), QStringLiteral("secure"),
                   QStringLiteral("enabled_notification_listeners")},
                  m_package + QLatin1Char('/') + QLatin1String(kListener), false});
    steps.append({QStringLiteral("battery-whitelist"),
                  {QStringLiteral("cmd"), QStringLiteral("deviceidle"), QStringLiteral("whitelist")},
                  QStringLiteral(",") + m_package + QLatin1Char(','), false});
    runSteps(serial, steps, true, std::move(done));
}

void PhoneSetup::runSteps(const QString &serial, QList<Step> steps, bool verify, Done done)
{
    if (!validSerial(serial)) {
        done(QJsonObject{{QStringLiteral("serial"), QString()},
                         {QStringLiteral("ok"), false},
                         {QStringLiteral("error"), QStringLiteral("bad-serial")},
                         {QStringLiteral("steps"), QJsonArray()}});
        return;
    }
    struct State {
        QList<Step> steps;
        qsizetype next = 0;
        QJsonArray report;
        bool allOk = true;
        Done done;
    };
    auto state = std::make_shared<State>();
    state->steps = std::move(steps);
    state->done = std::move(done);
    auto advance = std::make_shared<std::function<void()>>();
    *advance = [this, serial, verify, state, advance] {
        if (state->next >= state->steps.size()) {
            state->done(QJsonObject{{QStringLiteral("serial"), serial},
                                    {QStringLiteral("ok"), state->allOk},
                                    {QStringLiteral("steps"), state->report}});
            *advance = nullptr;  // break the self-reference
            return;
        }
        const Step step = state->steps.at(state->next++);
        QStringList args{QStringLiteral("-s"), serial, QStringLiteral("shell")};
        args += step.args;
        m_runner.run(args, [this, verify, state, advance, step](int code, const QByteArray &out) {
            auto record = [&](const QString &id, bool ok) {
                state->allOk = state->allOk && ok;
                state->report.append(QJsonObject{{QStringLiteral("id"), id},
                                                 {QStringLiteral("ok"), ok},
                                                 {QStringLiteral("code"), code}});
            };
            if (!verify) {
                record(step.id, code == 0);
            } else if (step.id == QLatin1String("package")) {
                // Output is inspected here and dropped; only booleans leave this function.
                const QString text = QString::fromUtf8(out);
                for (const QString &permission : permissions()) {
                    const QString needle = QStringLiteral("android.permission.%1: granted=true").arg(permission);
                    record(QStringLiteral("grant:") + permission, code == 0 && text.contains(needle));
                }
            } else {
                record(step.id, code == 0 && QString::fromUtf8(out).contains(step.expect));
            }
            (*advance)();
        });
    };
    (*advance)();
}

} // namespace QindaQt::Link
