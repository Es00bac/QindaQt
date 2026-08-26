// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinchromescenelifecycle.h"

#include <QtTest>

using namespace QindaQt::Compositor::KWinIntegration;

namespace {

class FakeCompositorLifecycle final : public QObject
{
    Q_OBJECT

public:
    void stop()
    {
        Q_EMIT aboutToToggleCompositing();
        sceneAlive = false;
        Q_EMIT compositingToggled(false);
    }

    void start()
    {
        Q_EMIT aboutToToggleCompositing();
        sceneAlive = true;
        Q_EMIT compositingToggled(true);
    }

    bool sceneAlive = true;

Q_SIGNALS:
    void aboutToToggleCompositing();
    void aboutToDestroy();
    void compositingToggled(bool active);
};

} // namespace

class KWinChromeSceneLifecycleTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void releasesSynchronouslyBeforeSceneDestructionAndRepublishesAfterStart();
    void compositorDestructionAndRepeatedStopAreIdempotent();
};

void KWinChromeSceneLifecycleTests::
    releasesSynchronouslyBeforeSceneDestructionAndRepublishesAfterStart()
{
    FakeCompositorLifecycle compositor;
    int releases = 0;
    int republishes = 0;
    bool releaseObservedLiveScene = false;
    bool republishObservedLiveScene = false;
    KWinChromeSceneLifecycle lifecycle(
        [&] {
            ++releases;
            releaseObservedLiveScene = compositor.sceneAlive;
        },
        [&] {
            ++republishes;
            republishObservedLiveScene = compositor.sceneAlive;
        });

    connect(&compositor, &FakeCompositorLifecycle::aboutToToggleCompositing,
            &lifecycle, &KWinChromeSceneLifecycle::prepareForSceneTeardown,
            Qt::DirectConnection);
    connect(&compositor, &FakeCompositorLifecycle::compositingToggled,
            &lifecycle, &KWinChromeSceneLifecycle::compositingToggled,
            Qt::DirectConnection);

    compositor.stop();
    QCOMPARE(releases, 1);
    QVERIFY(releaseObservedLiveScene);
    QVERIFY(!lifecycle.sceneAvailable());

    compositor.start();
    QCOMPARE(releases, 1);
    QCOMPARE(republishes, 1);
    QVERIFY(republishObservedLiveScene);
    QVERIFY(lifecycle.sceneAvailable());
}

void KWinChromeSceneLifecycleTests::
    compositorDestructionAndRepeatedStopAreIdempotent()
{
    FakeCompositorLifecycle compositor;
    int releases = 0;
    int republishes = 0;
    KWinChromeSceneLifecycle lifecycle([&] { ++releases; },
                                       [&] { ++republishes; });
    connect(&compositor, &FakeCompositorLifecycle::aboutToDestroy,
            &lifecycle, &KWinChromeSceneLifecycle::prepareForSceneTeardown,
            Qt::DirectConnection);
    connect(&compositor, &FakeCompositorLifecycle::aboutToToggleCompositing,
            &lifecycle, &KWinChromeSceneLifecycle::prepareForSceneTeardown,
            Qt::DirectConnection);
    connect(&compositor, &FakeCompositorLifecycle::compositingToggled,
            &lifecycle, &KWinChromeSceneLifecycle::compositingToggled,
            Qt::DirectConnection);

    Q_EMIT compositor.aboutToDestroy();
    compositor.stop();
    QCOMPARE(releases, 1);
    QCOMPARE(republishes, 0);
    QVERIFY(!lifecycle.sceneAvailable());

    compositor.start();
    QCOMPARE(republishes, 1);
    QVERIFY(lifecycle.sceneAvailable());
}

QTEST_MAIN(KWinChromeSceneLifecycleTests)
#include "tst_kwinchromescenelifecycle.moc"
