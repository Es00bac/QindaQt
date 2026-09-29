// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0286: the wallpaper controller picks each output's picture from the
// per-display and per-desktop choices, follows the current desktop, falls back
// for unknown or removed displays and desktops, and cross-fades with the
// theme's motion unless reduced motion is on. Settings1 arrives through a fake
// transport and the selection through a fake source: no bus, Display1, or
// compositor is involved.
#include "wallpapercontroller.h"
#include "wallpaperselectionsource.h"

#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/services/wallpaper_assignments/wallpaper_assignments.h"

#include <QColor>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;
using QindaQt::Services::WallpaperAssignments::WallpaperAssignments;

namespace {

constexpr auto kOwner = ":1.70";
constexpr auto kEpoch = "wallpaper-epoch";

class FakeTransport final : public SettingsTransport {
    Q_OBJECT
public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &, const QStringList &) override
    {
        tokens.append(token);
    }
    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override
    {
    }
    void requestActivation() override {}

    QList<quint64> tokens;
};

class FakeSelection final : public WallpaperSelectionSource {
    Q_OBJECT
public:
    const WallpaperAssignments &assignments() const override { return choices; }
    QString displayIdForConnector(const QString &connectorName) const override
    {
        return displays.value(connectorName);
    }
    QString currentDesktopId() const override { return desktop; }
    void publish() { Q_EMIT changed(); }

    WallpaperAssignments choices;
    QHash<QString, QString> displays;
    QString desktop;
};

QStringList scope()
{
    return {QStringLiteral("appearance.wallpaper"), QStringLiteral("appearance.wallpaperMode"),
            QStringLiteral("accessibility.reducedMotion")};
}

QVariantMap snapshotWire(quint64 revision, const QString &everywhere, bool reducedMotion)
{
    const QVariantMap values{{QStringLiteral("appearance.wallpaper"), everywhere},
                             {QStringLiteral("appearance.wallpaperMode"), QStringLiteral("scaled")},
                             {QStringLiteral("accessibility.reducedMotion"), reducedMotion}};
    QVariantMap sources;
    for (const QString &key : scope())
        sources.insert(key, QStringLiteral("user-overrides"));
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QString::fromLatin1(kEpoch)},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

// Answers the next snapshot request; false when none arrives.
[[nodiscard]] bool answer(FakeTransport &transport, quint64 revision,
                          const QString &everywhere, bool reducedMotion)
{
    if (!QTest::qWaitFor([&transport] { return !transport.tokens.isEmpty(); }, 2'000))
        return false;
    Q_EMIT transport.snapshotReceived(transport.tokens.takeFirst(), QString::fromLatin1(kOwner),
                                      snapshotWire(revision, everywhere, reducedMotion));
    return true;
}

// Publishes a newer confirmed revision the way Settings1 does: invalidate,
// let the client re-read, answer.
[[nodiscard]] bool confirm(FakeTransport &transport, quint64 revision,
                           const QString &everywhere, bool reducedMotion)
{
    Q_EMIT transport.settingsChanged(QString::fromLatin1(kOwner), QString::fromLatin1(kEpoch),
                                     revision, scope());
    return answer(transport, revision, everywhere, reducedMotion);
}

// Real (tiny) images, so every layer reaches Ready rather than Error.
[[nodiscard]] QString plant(const QTemporaryDir &directory, const QString &name, QColor color)
{
    const QString path = directory.filePath(name);
    QImage image(8, 8, QImage::Format_RGB32);
    image.fill(color);
    return image.save(path) ? path : QString();
}

// The background window the controller made for the first screen; every
// top-level Quick window in this process is one of its per-screen surfaces.
QQuickWindow *firstScreenWindow()
{
    const QScreen *screen = QGuiApplication::screens().value(0);
    for (QWindow *window : QGuiApplication::allWindows()) {
        auto *quick = qobject_cast<QQuickWindow *>(window);
        if (quick != nullptr && quick->screen() == screen)
            return quick;
    }
    return nullptr;
}

QUrl requested(const QQuickWindow *window)
{
    return window->property("wallpaperSource").toUrl();
}

QUrl presented(const QQuickWindow *window)
{
    return window->property("presentedSource").toUrl();
}

} // namespace

class WallpaperSelectionTests final : public QObject {
    Q_OBJECT

private slots:
    void choicesFollowTheCurrentDesktopWithFallbacks();
    void changedPicturesFadeUnlessMotionIsReduced();
};

void WallpaperSelectionTests::choicesFollowTheCurrentDesktopWithFallbacks()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString everywhere = plant(directory, QStringLiteral("everywhere.png"), Qt::darkGreen);
    const QString display = plant(directory, QStringLiteral("display.png"), Qt::darkBlue);
    const QString work = plant(directory, QStringLiteral("work.png"), Qt::darkRed);
    const QString shared = plant(directory, QStringLiteral("shared.png"), Qt::darkYellow);
    QVERIFY(!everywhere.isEmpty() && !display.isEmpty() && !work.isEmpty() && !shared.isEmpty());

    FakeTransport transport;
    SettingsClient client(transport, scope(),
                          {.requestTimeoutMilliseconds = 500,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10, 20}});
    auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(app != nullptr && !app->screens().isEmpty());
    const QString connector = app->screens().constFirst()->name();
    FakeSelection selection;
    QQmlEngine engine;
    WallpaperController controller(*app, engine, client, {directory.path()});
    controller.setSelectionSource(&selection);
    controller.start();
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QString::fromLatin1(kOwner));
    // Reduced motion: every change settles without a fade.
    QVERIFY(answer(transport, 1, everywhere, true));

    QQuickWindow *window = nullptr;
    QTRY_VERIFY((window = firstScreenWindow()) != nullptr);
    QTRY_COMPARE(requested(window), QUrl::fromLocalFile(everywhere));
    QCOMPARE(window->property("transitionMs").toInt(), 0);

    const QString left = QStringLiteral("edid:0123456789abcdef0123456789abcdef");
    const QString workDesktop = QStringLiteral("desktop-work");
    const QString playDesktop = QStringLiteral("desktop-play");
    QCOMPARE(int(selection.choices.set(left, QString(), display)), 0);
    QCOMPARE(int(selection.choices.set(left, workDesktop, work)), 0);
    QCOMPARE(int(selection.choices.set(QString(), playDesktop, shared)), 0);
    selection.desktop = workDesktop;
    selection.publish();
    // The output's identity is not known yet (Display1 silent): the desktop
    // has no every-display choice, so the everywhere wallpaper stays.
    QCOMPARE(requested(window), QUrl::fromLocalFile(everywhere));

    selection.displays.insert(connector, left);
    selection.publish();
    QCOMPARE(requested(window), QUrl::fromLocalFile(work));
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(work));

    // Switching desktops switches the picture on this output.
    selection.desktop = playDesktop;
    selection.publish();
    // The display's own choice outranks the desktop's every-display choice.
    QCOMPARE(requested(window), QUrl::fromLocalFile(display));
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(display));

    // Identity lost (unplugged and replaced, ambiguous twin): the desktop's
    // every-display choice applies; nothing saved was dropped.
    selection.displays.clear();
    selection.publish();
    QCOMPARE(requested(window), QUrl::fromLocalFile(shared));
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(shared));

    // A removed desktop falls back to the everywhere wallpaper.
    selection.desktop = QStringLiteral("desktop-removed");
    selection.publish();
    QCOMPARE(requested(window), QUrl::fromLocalFile(everywhere));
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(everywhere));

    // The display returns with its choices intact; an explicit empty choice
    // means no wallpaper there, not "inherit".
    QCOMPARE(int(selection.choices.set(left, workDesktop, QString())), 0);
    selection.displays.insert(connector, left);
    selection.desktop = workDesktop;
    selection.publish();
    QCOMPARE(requested(window), QUrl());
    QTRY_COMPARE(presented(window), QUrl());
    QCOMPARE(selection.choices.size(), 3);

    // A confirmed change of the everywhere wallpaper reaches only outputs
    // that follow it.
    selection.desktop = QStringLiteral("desktop-removed");
    selection.displays.clear();
    selection.publish();
    QVERIFY(confirm(transport, 2, display, true));
    QTRY_COMPARE(requested(window), QUrl::fromLocalFile(display));
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(display));
}

void WallpaperSelectionTests::changedPicturesFadeUnlessMotionIsReduced()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString first = plant(directory, QStringLiteral("first.png"), Qt::darkCyan);
    const QString second = plant(directory, QStringLiteral("second.png"), Qt::darkMagenta);
    const QString third = plant(directory, QStringLiteral("third.png"), Qt::darkGray);
    QVERIFY(!first.isEmpty() && !second.isEmpty() && !third.isEmpty());

    FakeTransport transport;
    SettingsClient client(transport, scope(),
                          {.requestTimeoutMilliseconds = 500,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10, 20}});
    auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(app != nullptr);
    QQmlEngine engine;
    WallpaperController controller(*app, engine, client, {directory.path()});
    controller.setMotionDuration(150);
    controller.start();
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QString::fromLatin1(kOwner));
    QVERIFY(answer(transport, 1, first, false));

    QQuickWindow *window = nullptr;
    QTRY_VERIFY((window = firstScreenWindow()) != nullptr);
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(first));

    // Motion allowed: the new picture loads and fades in over the old one,
    // which stays presented until the fade has finished.
    QVERIFY(confirm(transport, 2, second, false));
    QTRY_COMPARE(requested(window), QUrl::fromLocalFile(second));
    QCOMPARE(window->property("transitionMs").toInt(), 150);
    QCOMPARE(presented(window), QUrl::fromLocalFile(first));
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(second));

    // Reduced motion: the same change swaps without any fade.
    QVERIFY(confirm(transport, 3, third, true));
    QTRY_COMPARE(requested(window), QUrl::fromLocalFile(third));
    QCOMPARE(window->property("transitionMs").toInt(), 0);
    QTRY_COMPARE(presented(window), QUrl::fromLocalFile(third));
}

int runWallpaperSelectionTests(int argc, char **argv)
{
    WallpaperSelectionTests selection;
    return QTest::qExec(&selection, argc, argv);
}

#include "tst_wallpaperselection.moc"
