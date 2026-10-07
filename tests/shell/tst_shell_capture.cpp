// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
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
    void capturesScaledResolution_data();
    void capturesScaledResolution();
    void refusesUnwritableOutput();
    void rejectsOversizedGeometry_data();
    void rejectsOversizedGeometry();
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
    // The Bliss pairing exercises the worn Luna taskbar presentation end to
    // end: profile-selected taskbar dressing over the Bliss token palette.
    QTest::newRow("qinda-bliss-1080p") << QSize(1920, 1080)
                                       << QStringLiteral("qinda-bliss")
                                       << QStringLiteral("qinda-bliss");
    // ADR-0268: each desktop experience with the theme it pairs with (Menu
    // and Dock and Classic Taskbar are the two rows above).
    const struct {
        const char *profile;
        const char *theme;
    } experiences[] = {{"windows-modern", "qinda-daylight"},
                       {"beos-inspired", "qinda-marigold"},
                       {"win31-inspired", "qinda-classic-grey"},
                       {"nextstep-inspired", "qinda-graphite"}};
    for (const auto &experience : experiences) {
        const QByteArray tag = QByteArray("experience-") + experience.profile;
        QTest::newRow(tag.constData()) << QSize(1920, 1080)
                                       << QString::fromLatin1(experience.profile)
                                       << QString::fromLatin1(experience.theme);
    }
    // Dispatcher/layout changes affect every preset, including side panels
    // and legacy controls migrated into compiled applets.
    const QStringList otherProfiles{
        QStringLiteral("gnome-inspired"), QStringLiteral("minimal"),
        QStringLiteral("nextstep-inspired"), QStringLiteral("unity-inspired"),
        QStringLiteral("windows-modern"), QStringLiteral("xfce-inspired"),
        QStringLiteral("qinda-bliss"), QStringLiteral("beos-inspired"),
        QStringLiteral("win31-inspired")};
    for (const auto &preset : otherProfiles) {
        QTest::newRow(qPrintable(preset)) << QSize(1920, 1080) << preset
                                        << QStringLiteral("qinda-dark");
    }
    // Theming v2 (ADR-0206): every stock profile under one translucent dark
    // theme and one opaque light theme, so panel, popup and chrome materials
    // are captured over each dressing.
    const QStringList everyProfile{
        QStringLiteral("qindaqt"), QStringLiteral("macos-inspired"),
        QStringLiteral("gnome-inspired"), QStringLiteral("minimal"),
        QStringLiteral("nextstep-inspired"), QStringLiteral("unity-inspired"),
        QStringLiteral("windows-modern"), QStringLiteral("xfce-inspired"),
        QStringLiteral("qinda-bliss"), QStringLiteral("beos-inspired"),
        QStringLiteral("win31-inspired")};
    for (const auto *theme : {"qinda-glass-dark", "qinda-paper"}) {
        for (const auto &preset : everyProfile) {
            const QByteArray tag = QByteArray(theme) + '-' + preset.toLatin1();
            QTest::newRow(tag.constData()) << QSize(1920, 1080) << preset
                                          << QString::fromLatin1(theme);
        }
    }
}

static void verifyPreviewCapture(QSize resolution, const QString &profile,
                                 const QString &theme, qreal scale)
{
    QTemporaryDir outputDirectory(
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("shell-capture-XXXXXX")));
    QVERIFY2(outputDirectory.isValid(), "Could not create a temporary capture directory");
    const QString outputPath = outputDirectory.filePath(QStringLiteral("nested/preview.png"));

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    environment.insert(QStringLiteral("QSG_RENDER_LOOP"), QStringLiteral("basic"));
    environment.insert(QStringLiteral("QT_SCALE_FACTOR"), QString::number(scale));
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent"));
    environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent"));
    // Both the production dispatcher and hosted applet imports execute in
    // this process. Undefined tokens and delegate binding errors must abort
    // the row instead of being hidden behind a superficially valid PNG.
    environment.insert(QStringLiteral("QT_FATAL_WARNINGS"), QString::number(scale));
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
    QCOMPARE(reader.size(), resolution * scale);
    QVERIFY2(!reader.read().isNull(), qPrintable(reader.errorString()));
}

void ShellCaptureTest::capturesRequiredResolution()
{
    QFETCH(QSize, resolution);
    QFETCH(QString, profile);
    QFETCH(QString, theme);
    verifyPreviewCapture(resolution, profile, theme, 1);
}

void ShellCaptureTest::capturesScaledResolution_data()
{
    QTest::addColumn<QSize>("logicalSize");
    QTest::addColumn<qreal>("scale");
    QTest::newRow("native-2x") << QSize(1280, 720) << qreal(2);
    QTest::newRow("fractional-1.25") << QSize(1280, 720) << qreal(1.25);
    QTest::newRow("fractional-1.5-odd") << QSize(1281, 721) << qreal(1.5);
}

void ShellCaptureTest::capturesScaledResolution()
{
    QFETCH(QSize, logicalSize);
    QFETCH(qreal, scale);
    verifyPreviewCapture(logicalSize, QStringLiteral("qindaqt"),
                         QStringLiteral("qinda-dark"), scale);
}

void ShellCaptureTest::refusesUnwritableOutput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blockedParent = directory.filePath(QStringLiteral("file-not-directory"));
    QFile occupied(blockedParent);
    QVERIFY(occupied.open(QIODevice::WriteOnly));
    occupied.close();
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    environment.insert(QStringLiteral("QSG_RENDER_LOOP"), QStringLiteral("basic"));
    environment.insert(QStringLiteral("QT_SCALE_FACTOR"), QStringLiteral("1"));
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent"));
    environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent"));
    process.setProcessEnvironment(environment);
    const QString output = blockedParent + QStringLiteral("/preview.png");
    process.start(QStringLiteral(QINDAQT_SHELL_PREVIEW_EXECUTABLE),
                  {QStringLiteral("--width"), QStringLiteral("640"),
                   QStringLiteral("--height"), QStringLiteral("480"),
                   QStringLiteral("--screenshot"), output});
    QVERIFY(process.waitForStarted(5000));
    QVERIFY(process.waitForFinished(15000));
    const QByteArray diagnostics = process.readAllStandardOutput() + process.readAllStandardError();
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 4);
    QVERIFY2(diagnostics.contains("Cannot create screenshot directory"), diagnostics.constData());
    QVERIFY(!QFileInfo::exists(output));
}

void ShellCaptureTest::rejectsOversizedGeometry_data()
{
    QTest::addColumn<QString>("width");
    QTest::addColumn<QString>("height");
    QTest::addColumn<QString>("scale");
    QTest::addColumn<QByteArray>("reason");
    QTest::newRow("logical-axis") << QStringLiteral("2147483647")
        << QStringLiteral("480") << QStringLiteral("1")
        << QByteArray("Preview dimensions");
    QTest::newRow("logical-area") << QStringLiteral("10000")
        << QStringLiteral("10000") << QStringLiteral("1")
        << QByteArray("Preview dimensions");
    QTest::newRow("native-before-window") << QStringLiteral("640")
        << QStringLiteral("480") << QStringLiteral("100")
        << QByteArray("Preview native geometry");
}

void ShellCaptureTest::rejectsOversizedGeometry()
{
    QFETCH(QString, width);
    QFETCH(QString, height);
    QFETCH(QString, scale);
    QFETCH(QByteArray, reason);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString output = directory.filePath(QStringLiteral("absent.png"));
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    environment.insert(QStringLiteral("QT_SCALE_FACTOR"), scale);
    environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent"));
    environment.insert(QStringLiteral("DBUS_SYSTEM_BUS_ADDRESS"), QStringLiteral("unix:path=/nonexistent"));
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral(QINDAQT_SHELL_PREVIEW_EXECUTABLE),
                  {QStringLiteral("--width"), width, QStringLiteral("--height"), height,
                   QStringLiteral("--screenshot"), output});
    QVERIFY(process.waitForStarted(5000));
    QVERIFY(process.waitForFinished(5000));
    const QByteArray diagnostics = process.readAllStandardOutput() + process.readAllStandardError();
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 2);
    QVERIFY2(diagnostics.contains(reason), diagnostics.constData());
    QVERIFY(!QFileInfo::exists(output));
    QVERIFY(!diagnostics.contains("Saved screenshot"));
}

QTEST_GUILESS_MAIN(ShellCaptureTest)
#include "tst_shell_capture.moc"
