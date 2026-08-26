// SPDX-License-Identifier: GPL-3.0-or-later
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSize>
#include <QTemporaryDir>
#include <QTest>

class ShellCaptureTest final : public QObject {
    Q_OBJECT

private slots:
    void capturesRequiredResolution_data();
    void capturesRequiredResolution();
};

void ShellCaptureTest::capturesRequiredResolution_data()
{
    QTest::addColumn<QSize>("resolution");
    QTest::addColumn<QString>("profile");
    QTest::addColumn<QString>("theme");
    QTest::newRow("1080p") << QSize(1920, 1080) << QStringLiteral("qindaqt")
                            << QStringLiteral("qinda-dark");
    QTest::newRow("wuxga") << QSize(1920, 1200) << QStringLiteral("qindaqt")
                           << QStringLiteral("qinda-dark");
    QTest::newRow("1440p") << QSize(2560, 1440) << QStringLiteral("qindaqt")
                           << QStringLiteral("qinda-dark");
    QTest::newRow("qinda-macos-wuxga") << QSize(1920, 1200)
                                       << QStringLiteral("macos-inspired")
                                       << QStringLiteral("qinda-macos");
}

void ShellCaptureTest::capturesRequiredResolution()
{
    QFETCH(QSize, resolution);
    QFETCH(QString, profile);
    QFETCH(QString, theme);

    QTemporaryDir outputDirectory;
    QVERIFY2(outputDirectory.isValid(), "Could not create a temporary capture directory");
    const QString outputPath = outputDirectory.filePath(QStringLiteral("nested/preview.png"));

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    environment.insert(QStringLiteral("QSG_RENDER_LOOP"), QStringLiteral("basic"));
    environment.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("1"));
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(QINDAQT_SHELL_PREVIEW_EXECUTABLE),
                  {QStringLiteral("--profile"),
                   profile,
                   QStringLiteral("--theme"),
                   theme,
                   QStringLiteral("--width"),
                   QString::number(resolution.width()),
                   QStringLiteral("--height"),
                   QString::number(resolution.height()),
                   QStringLiteral("--screenshot"),
                   outputPath});

    QVERIFY2(process.waitForStarted(5'000), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(15'000), qPrintable(process.errorString()));
    const QByteArray diagnostics = process.readAllStandardOutput() + process.readAllStandardError();
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(process.exitCode() == 0, diagnostics.constData());
    QVERIFY2(QFileInfo::exists(outputPath), diagnostics.constData());

    QImageReader reader(outputPath);
    QCOMPARE(reader.format(), QByteArray("png"));
    QCOMPARE(reader.size(), resolution);
    QVERIFY2(!reader.read().isNull(), qPrintable(reader.errorString()));
}

QTEST_GUILESS_MAIN(ShellCaptureTest)
#include "tst_shell_capture.moc"
