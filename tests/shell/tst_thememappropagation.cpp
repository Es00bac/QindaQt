// SPDX-License-Identifier: GPL-3.0-or-later
#include "thememappropagation.h"

#include <QQuickWindow>
#include <QtTest>

#include <memory>

// The raw panel/notification `theme` maps are plain dynamic properties on the
// QML window roots so a confirmed Settings1 appearance change can reach
// already-created surfaces without a restart.
class ThemeMapPropagationTests final : public QObject {
    Q_OBJECT

private slots:
    void updatesLiveWindowsAndPrunesDestroyed();
};

void ThemeMapPropagationTests::updatesLiveWindowsAndPrunesDestroyed()
{
    const QVariantMap initial{{QStringLiteral("accent"), QStringLiteral("#111111")}};
    const QVariantMap updated{{QStringLiteral("accent"), QStringLiteral("#222222")}};

    auto first = std::make_unique<QQuickWindow>();
    auto second = std::make_unique<QQuickWindow>();
    first->setProperty("theme", initial);
    second->setProperty("theme", initial);

    QList<QPointer<QQuickWindow>> windows{QPointer<QQuickWindow>(first.get()),
                                          QPointer<QQuickWindow>(second.get())};
    QindaQt::Shell::propagateThemeMapToWindows(updated, windows);
    QCOMPARE(first->property("theme").toMap().value(QStringLiteral("accent")).toString(),
             QStringLiteral("#222222"));
    QCOMPARE(second->property("theme").toMap().value(QStringLiteral("accent")).toString(),
             QStringLiteral("#222222"));
    QCOMPARE(windows.size(), 2);

    // A destroyed surface is pruned instead of dereferenced.
    second.reset();
    QindaQt::Shell::propagateThemeMapToWindows(initial, windows);
    QCOMPARE(windows.size(), 1);
    QCOMPARE(first->property("theme").toMap().value(QStringLiteral("accent")).toString(),
             QStringLiteral("#111111"));
}

QTEST_MAIN(ThemeMapPropagationTests)
#include "tst_thememappropagation.moc"
