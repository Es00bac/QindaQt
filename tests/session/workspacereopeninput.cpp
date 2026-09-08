// SPDX-License-Identifier: GPL-3.0-or-later
#include "workspacereopeninput.h"

#include "hybridtestinputdriver.h"

#include <QJsonDocument>

namespace QindaQt::Test {
namespace {

constexpr int KeySettleMilliseconds = 90;

QJsonObject pointerEvent(const QPointF &position)
{
    return {{QStringLiteral("type"), QStringLiteral("pointer-absolute")},
            {QStringLiteral("x"), position.x()},
            {QStringLiteral("y"), position.y()}};
}

QJsonObject keyEvent(const QString &name, bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("key")},
            {QStringLiteral("key"), name},
            {QStringLiteral("pressed"), pressed}};
}

QJsonObject buttonEvent(QLatin1StringView name, bool pressed)
{
    return {{QStringLiteral("type"), QStringLiteral("button")},
            {QStringLiteral("button"), name},
            {QStringLiteral("pressed"), pressed}};
}

} // namespace

WorkspaceReopenInput::WorkspaceReopenInput(CompositorProbeClient &client)
    : m_client(client)
{
}

bool WorkspaceReopenInput::movePointer(const QPointF &point, QString *error)
{
    return inject({pointerEvent(point)}, error);
}

bool WorkspaceReopenInput::clickButton(QLatin1StringView button, QString *error)
{
    if (!inject({buttonEvent(button, true)}, error)) {
        return false;
    }
    processProbeEventsFor(40);
    return inject({buttonEvent(button, false)}, error);
}

bool WorkspaceReopenInput::pressKey(const QString &key, QString *error)
{
    return pressChord({key}, error);
}

bool WorkspaceReopenInput::pressChord(const QStringList &keys, QString *error)
{
    if (keys.isEmpty()) {
        *error = QStringLiteral("workspace reopen input chord is empty");
        return false;
    }
    QJsonArray events;
    for (const QString &key : keys) {
        events.append(keyEvent(key, true));
    }
    for (auto iterator = keys.crbegin(); iterator != keys.crend(); ++iterator) {
        events.append(keyEvent(*iterator, false));
    }
    return inject(events, error);
}

bool WorkspaceReopenInput::drag(const QPointF &start, const QPointF &end,
                                bool metaShift, QString *error)
{
    if (!inject({pointerEvent(start)}, error)) {
        return false;
    }
    processProbeEventsFor(30);
    if (metaShift
        && !inject({keyEvent(QStringLiteral("left-meta"), true),
                    keyEvent(QStringLiteral("left-shift"), true)}, error)) {
        return false;
    }
    if (!inject({buttonEvent(QLatin1StringView("left"), true)}, error)) {
        return false;
    }
    constexpr int steps = 12;
    for (int step = 1; step <= steps; ++step) {
        const auto progress = qreal(step) / qreal(steps);
        if (!inject({pointerEvent(start + ((end - start) * progress))}, error)) {
            return false;
        }
        processProbeEventsFor(15);
    }
    if (!inject({buttonEvent(QLatin1StringView("left"), false)}, error)) {
        return false;
    }
    if (metaShift
        && !inject({keyEvent(QStringLiteral("left-shift"), false),
                    keyEvent(QStringLiteral("left-meta"), false)}, error)) {
        return false;
    }
    processProbeEventsFor(60);
    return true;
}

bool WorkspaceReopenInput::inject(const QJsonArray &events, QString *error)
{
    const QJsonObject request{{QStringLiteral("schemaVersion"), 1},
                              {QStringLiteral("events"), events}};
    const auto reply = m_client.call(
        QStringLiteral("InjectTestInput"),
        QJsonDocument(request).toJson(QJsonDocument::Compact), error);
    if (!reply) {
        return false;
    }
    if (reply->value(QStringLiteral("status")) != QStringLiteral("injected")
        || reply->value(QStringLiteral("eventCount")).toInt(-1) != events.size()
        || reply->value(QStringLiteral("deviceId")).toString().isEmpty()) {
        *error = QStringLiteral("development input endpoint rejected a valid event batch: %1")
                     .arg(QString::fromUtf8(
                         QJsonDocument(*reply).toJson(QJsonDocument::Compact)));
        return false;
    }
    const auto replyDeviceId = reply->value(QStringLiteral("deviceId")).toString();
    if (!m_deviceId.isEmpty() && m_deviceId != replyDeviceId) {
        *error = QStringLiteral(
            "development input device identity changed during one session");
        return false;
    }
    m_deviceId = replyDeviceId;
    ++m_requestCount;
    return true;
}

bool pressSequence(WorkspaceReopenInput &driver, const QStringList &keys,
                   QString *error)
{
    for (const QString &key : keys) {
        if (!driver.pressKey(key, error)) {
            return false;
        }
        processProbeEventsFor(KeySettleMilliseconds);
    }
    return true;
}

bool typeFixtureName(WorkspaceReopenInput &driver, const QString &text,
                     QString *error)
{
    // The development key codec intentionally carries only a handful of
    // letters (c/n/v). The fixture name is built from exactly those, so the
    // rename dialog receives real text input through the production path.
    for (const QChar &character : text) {
        const auto name = QString(character).toLower();
        if (name != QStringLiteral("c") && name != QStringLiteral("n")
            && name != QStringLiteral("v")) {
            *error = QStringLiteral(
                "workspace fixture name '%1' needs a key outside the codec")
                         .arg(text);
            return false;
        }
        if (!pressSequence(driver, {name}, error)) {
            return false;
        }
    }
    return true;
}

bool openMenuAt(WorkspaceReopenInput &driver, const QPointF &point,
                QString *error)
{
    if (!driver.movePointer(point, error)) {
        return false;
    }
    processProbeEventsFor(60);
    if (!driver.clickButton(QLatin1StringView("right"), error)) {
        return false;
    }
    processProbeEventsFor(250);
    return true;
}

bool chooseMenuItem(WorkspaceReopenInput &driver, int downs, QString *error)
{
    for (int step = 0; step < downs; ++step) {
        if (!pressSequence(driver, {QStringLiteral("down")}, error)) {
            return false;
        }
    }
    return pressSequence(driver, {QStringLiteral("enter")}, error);
}

bool chooseSubmenuItem(WorkspaceReopenInput &driver, int menuDowns,
                       int submenuDowns, QString *error)
{
    for (int step = 0; step < menuDowns; ++step) {
        if (!pressSequence(driver, {QStringLiteral("down")}, error)) {
            return false;
        }
    }
    if (!pressSequence(driver, {QStringLiteral("right")}, error)) {
        return false;
    }
    return chooseMenuItem(driver, submenuDowns, error);
}

std::optional<ObservedWindow> awaitWindowTitle(CompositorProbeClient &client,
                                               const QString &title,
                                               QString *error,
                                               int timeoutMilliseconds)
{
    const auto inventory = client.awaitWindows(
        {title},
        [title](const WindowInventory &windows) {
            const auto found = windows.constFind(title);
            return found != windows.cend() && found->frame.isValid();
        },
        error, timeoutMilliseconds);
    if (!inventory) {
        return std::nullopt;
    }
    return inventory->value(title);
}

bool awaitWindowTitleGone(CompositorProbeClient &client, const QString &title,
                          QString *error, int timeoutMilliseconds)
{
    // awaitWindows keys its result on requested titles; detecting a departure
    // needs the raw inventory array instead.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        const auto windows = client.windows(error);
        if (!windows) {
            return false;
        }
        bool present = false;
        for (const auto &entry : *windows) {
            if (entry.toObject().value(QStringLiteral("title")).toString()
                == title) {
                present = true;
                break;
            }
        }
        if (!present) {
            return true;
        }
        processProbeEventsFor(60);
    }
    *error = QStringLiteral("window '%1' still present after %2 ms")
                 .arg(title)
                 .arg(timeoutMilliseconds);
    return false;
}

std::optional<QJsonArray> awaitEligibleWindows(
    CompositorProbeClient &client, const QStringList &expectedTitles,
    QString *error, int timeoutMilliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMilliseconds) {
        const auto windows = client.windows(error);
        if (!windows) {
            return std::nullopt;
        }
        QJsonArray eligible;
        for (const auto &entry : *windows) {
            const auto object = entry.toObject();
            if (object.value(QStringLiteral("containerId")).toString().isEmpty()
                && !object.value(QStringLiteral("minimized")).toBool()
                && !object.value(QStringLiteral("hidden")).toBool()) {
                eligible.append(object);
            }
        }
        bool allPresent = true;
        for (const QString &title : expectedTitles) {
            bool found = false;
            for (const auto &entry : eligible) {
                if (entry.toObject().value(QStringLiteral("title")).toString()
                    == title) {
                    found = true;
                    break;
                }
            }
            allPresent = allPresent && found;
        }
        if (allPresent) {
            return eligible;
        }
        processProbeEventsFor(60);
    }
    *error = QStringLiteral(
        "eligible inventory never contained every expected title: %1")
                 .arg(expectedTitles.join(QLatin1Char(',')));
    return std::nullopt;
}

} // namespace QindaQt::Test
