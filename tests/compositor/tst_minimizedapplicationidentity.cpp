// SPDX-License-Identifier: GPL-3.0-or-later
#include "minimizedapplicationidentity.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

class MinimizedApplicationIdentityTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void desktopMetadataLeadsIconAndApplicationLabel();
    void desktopFileAndClassFallbacksRemainMeaningful();
    void qualifiedApplicationIdsUseTheirHumanFinalComponent();
    void genericWaylandIconsAreNeverApplicationCandidates();
};

void MinimizedApplicationIdentityTests::desktopMetadataLeadsIconAndApplicationLabel()
{
    const auto identity = resolveMinimizedApplicationIdentity(
        QStringLiteral("org.example.Editor.desktop"), QStringLiteral("editor"),
        QStringLiteral("Example Editor"), QStringLiteral("example-editor"),
        QStringLiteral("notes.txt"));
    QCOMPARE(identity.applicationId, QStringLiteral("org.example.Editor.desktop"));
    QCOMPARE(identity.label, QStringLiteral("Example Editor"));
    QCOMPARE(identity.iconThemeCandidates,
             QStringList({QStringLiteral("example-editor"),
                          QStringLiteral("org.example.Editor"),
                          QStringLiteral("editor")}));
}


void MinimizedApplicationIdentityTests::qualifiedApplicationIdsUseTheirHumanFinalComponent()
{
    const auto fallback = resolveMinimizedApplicationIdentity(
        {}, QStringLiteral("org.qindaqt.gather.Fallback"), {}, {},
        QStringLiteral("Fallback Fixture"));
    QCOMPARE(fallback.applicationId, QStringLiteral("org.qindaqt.gather.Fallback"));
    QCOMPARE(fallback.label, QStringLiteral("Fallback"));
    QCOMPARE(fallback.iconThemeCandidates,
             QStringList({QStringLiteral("org.qindaqt.gather.Fallback")}));

    const auto simple = resolveMinimizedApplicationIdentity(
        {}, QStringLiteral("Firefox"), {}, {}, QStringLiteral("Private window"));
    QCOMPARE(simple.label, QStringLiteral("Firefox"));

    const auto trailingDot = resolveMinimizedApplicationIdentity(
        {}, QStringLiteral("org.example."), {}, {}, QStringLiteral("Project window"));
    QCOMPARE(trailingDot.applicationId, QStringLiteral("org.example."));
    QCOMPARE(trailingDot.label, QStringLiteral("Project window"));
}

void MinimizedApplicationIdentityTests::genericWaylandIconsAreNeverApplicationCandidates()
{
    const auto identity = resolveMinimizedApplicationIdentity(
        QStringLiteral("org.example.Browser.desktop"), QStringLiteral("wayland"),
        QStringLiteral("Example Browser"), QStringLiteral("application-x-executable"),
        QStringLiteral("Private window"));
    QCOMPARE(identity.iconThemeCandidates,
             QStringList({QStringLiteral("org.example.Browser")}));
    QVERIFY(isGenericMinimizedIconName(QStringLiteral("wayland")));
    QVERIFY(isGenericMinimizedIconName(QStringLiteral("application-x-executable")));
    QVERIFY(!isGenericMinimizedIconName(QStringLiteral("example-browser")));
}

void MinimizedApplicationIdentityTests::desktopFileAndClassFallbacksRemainMeaningful()
{
    const auto desktop = resolveMinimizedApplicationIdentity(
        QStringLiteral("org.example.Clock.desktop"), QStringLiteral("wayland"),
        {}, {}, QStringLiteral("Alarm"));
    QCOMPARE(desktop.label, QStringLiteral("Clock"));
    QCOMPARE(desktop.iconThemeCandidates,
             QStringList({QStringLiteral("org.example.Clock")}));

    const auto appClass = resolveMinimizedApplicationIdentity(
        {}, QStringLiteral("Firefox"), {}, {}, QStringLiteral("Report"));
    QCOMPARE(appClass.applicationId, QStringLiteral("Firefox"));
    QCOMPARE(appClass.label, QStringLiteral("Firefox"));

    const auto caption = resolveMinimizedApplicationIdentity(
        {}, QStringLiteral("wayland"), {}, {}, QStringLiteral("Alarm"));
    QCOMPARE(caption.label, QStringLiteral("Alarm"));
}

QTEST_APPLESS_MAIN(MinimizedApplicationIdentityTests)
#include "tst_minimizedapplicationidentity.moc"
