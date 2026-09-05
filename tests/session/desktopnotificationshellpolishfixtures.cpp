// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopnotificationshellpolishfixtures.h"

#include <utility>

namespace QindaQt::Test {

QJsonObject desktopNotificationShellTaskListFixture()
{
    return {
        {QStringLiteral("phase"), QStringLiteral("ready")},
        {QStringLiteral("generation"), QStringLiteral("1")},
        {QStringLiteral("windowCount"), 2},
        {QStringLiteral("buttons"),
         QJsonArray{
             QJsonObject{{QStringLiteral("applicationId"), QStringLiteral("org.qindaqt.Settings")},
                         {QStringLiteral("iconName"), QStringLiteral("preferences-system")},
                         {QStringLiteral("iconResolved"), true}},
             QJsonObject{{QStringLiteral("applicationId"), QStringLiteral("org.qindaqt.TextEditor")},
                         {QStringLiteral("iconName"), QStringLiteral("accessories-text-editor")},
                         {QStringLiteral("iconResolved"), true}},
         }},
    };
}

QJsonObject desktopNotificationShellQuietingFixture()
{
    return {{QStringLiteral("enabled"), false},
            {QStringLiteral("hasBaseline"), true},
            {QStringLiteral("state"), QStringLiteral("ready")},
            {QStringLiteral("canToggle"), true},
            {QStringLiteral("statusText"), QString{}},
            {QStringLiteral("errorText"), QString{}}};
}

QJsonArray desktopNotificationShellPanelAppletsFixture()
{
    return {
        QJsonObject{{QStringLiteral("panelId"), QStringLiteral("smart-shelf")},
                    {QStringLiteral("appletId"), QStringLiteral("apps")},
                    {QStringLiteral("plugin"), QStringLiteral("launcher")},
                    {QStringLiteral("ready"), true},
                    {QStringLiteral("entryPoint"), QStringLiteral("qindaqt.applets.launcher")}},
        QJsonObject{{QStringLiteral("panelId"), QStringLiteral("smart-shelf")},
                    {QStringLiteral("appletId"), QStringLiteral("hosted-task-list")},
                    {QStringLiteral("plugin"), QStringLiteral("task-list")},
                    {QStringLiteral("ready"), true},
                    {QStringLiteral("entryPoint"), QStringLiteral("qindaqt.applets.task-list")}},
    };
}

QJsonObject desktopNotificationShellSnapshotFixture(
    bool privatePresentationAllowed, bool centerOpen, bool exists, bool visible,
    QString outputName, QString centerOpenedCount)
{
    QJsonObject center{{QStringLiteral("exists"), exists}};
    if (exists) {
        center.insert(QStringLiteral("visible"), visible);
        center.insert(QStringLiteral("outputName"), std::move(outputName));
        center.insert(QStringLiteral("geometry"),
                      QJsonObject{{QStringLiteral("x"), 824},
                                  {QStringLiteral("y"), 46},
                                  {QStringLiteral("width"), 440},
                                  {QStringLiteral("height"), 640}});
    }
    return {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("shellPid"), QStringLiteral("53")},
        {QStringLiteral("tokens"),
         QJsonObject{{QStringLiteral("ready"), true},
                     {QStringLiteral("qstRevision"), 1},
                     {QStringLiteral("generation"), QStringLiteral("1")},
                     {QStringLiteral("sourceThemeId"), QStringLiteral("qinda-dark")},
                     {QStringLiteral("backgroundBase"), QStringLiteral("#171a18")}}},
        {QStringLiteral("taskList"), desktopNotificationShellTaskListFixture()},
        {QStringLiteral("quieting"), desktopNotificationShellQuietingFixture()},
        {QStringLiteral("panelApplets"), desktopNotificationShellPanelAppletsFixture()},
        {QStringLiteral("presentation"),
         QJsonObject{{QStringLiteral("privatePresentationAllowed"), privatePresentationAllowed},
                     {QStringLiteral("centerOpen"), centerOpen}}},
        {QStringLiteral("windows"), QJsonObject{{QStringLiteral("center"), center}}},
        {QStringLiteral("observations"),
         QJsonObject{{QStringLiteral("centerOpenedCount"), std::move(centerOpenedCount)}}},
    };
}

void mutateDesktopNotificationShellPolishFixture(QJsonObject &snapshot,
                                                  const QString &mutation)
{
    if (mutation == QStringLiteral("quieting")) {
        auto quieting = snapshot.value(QStringLiteral("quieting")).toObject();
        quieting.insert(QStringLiteral("state"), QStringLiteral("unavailable"));
        snapshot.insert(QStringLiteral("quieting"), quieting);
        return;
    }
    auto applets = snapshot.value(QStringLiteral("panelApplets")).toArray();
    auto applet = applets[0].toObject();
    applet.insert(QStringLiteral("plugin"), QStringLiteral("application-launcher"));
    applets[0] = applet;
    snapshot.insert(QStringLiteral("panelApplets"), applets);
}

} // namespace QindaQt::Test
