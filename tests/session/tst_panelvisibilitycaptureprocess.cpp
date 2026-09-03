// SPDX-License-Identifier: GPL-3.0-or-later
#include "panelvisibilitycaptureprocess.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QTest>

using namespace QindaQt::Test::PanelVisibilityCapture;

class PanelVisibilityCaptureProcessTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void appliesLoaderPathOnlyToConfiguredChild()
    {
        QProcessEnvironment base;
        base.insert(QStringLiteral("LD_LIBRARY_PATH"), QStringLiteral("/system/lib"));
        base.insert(QStringLiteral("PROOF_SENTINEL"), QStringLiteral("retained"));
        QProcess process;
        QString failure;

        QVERIFY(configureCaptureProcess(
            process, base, QStringLiteral("/private/bin/weston-screenshooter"),
            QStringLiteral("/private/lib:/private/lib/weston"), &failure));
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QCOMPARE(process.program(), QStringLiteral("/private/bin/weston-screenshooter"));
        const auto applied = process.processEnvironment();
        QCOMPARE(applied.value(QStringLiteral("WAYLAND_DISPLAY")),
                 QStringLiteral("qindaqt-parent-wayland"));
        QCOMPARE(applied.value(QStringLiteral("LD_LIBRARY_PATH")),
                 QStringLiteral("/private/lib:/private/lib/weston:/system/lib"));
        QCOMPARE(applied.value(QStringLiteral("PROOF_SENTINEL")),
                 QStringLiteral("retained"));
        QCOMPARE(base.value(QStringLiteral("LD_LIBRARY_PATH")),
                 QStringLiteral("/system/lib"));
    }

    void rejectsMalformedLoaderPathWithoutConfiguringProcess()
    {
        for (const QString &path : {
                 QString{}, QStringLiteral("relative/lib"),
                 QStringLiteral("/private/lib:"),
             }) {
            QProcess process;
            QString failure;
            QVERIFY(!configureCaptureProcess(
                process, {}, QStringLiteral("/private/bin/weston-screenshooter"),
                path, &failure));
            QVERIFY(failure.contains(QStringLiteral("exact and absolute")));
            QVERIFY(process.program().isEmpty());
            QVERIFY(process.processEnvironment().isEmpty());
        }
    }
};

QTEST_APPLESS_MAIN(PanelVisibilityCaptureProcessTest)

#include "tst_panelvisibilitycaptureprocess.moc"
