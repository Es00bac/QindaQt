// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>

#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>

#include <QSignalSpy>
#include <QtTest>

using QindaQt::Apps::SettingsScreensaver::ProcessScreensaverPreview;
using QindaQt::Apps::SettingsScreensaver::ScreensaverPreview;
using QindaQt::Session::DesktopControls::ScreensaverCatalog;
using QindaQt::Session::DesktopControls::ScreensaverCatalogEntry;
using QindaQt::Session::DesktopControls::ScreensaverPreferences;

namespace {

// A fixed installed set plus one token ("qinda-slowdouble") that names a real
// harmless program, so the "refuses to stack previews" row can run an actual
// process without involving the locker.
class FakeCatalog final : public ScreensaverCatalog {
public:
    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override
    {
        return {
            {QStringLiteral("qinda-patrol"), QStringLiteral("Qinda Patrol"),
             {}, {}, {QStringLiteral("--screensaver")}, true},
            {QStringLiteral("prism-brawl"), QStringLiteral("Prism Brawl"),
             {}, {}, {QStringLiteral("--screensaver")}, false},
            {QStringLiteral("qinda-slowdouble"), QStringLiteral("Slow Double"),
             {}, {}, {QStringLiteral("2")}, false},
        };
    }
};

} // namespace

class ScreensaverPreviewTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void kindForNothingChosenIsUnavailable();
    void kindForUnknownTokenIsUnavailable();
    void blankNeedsTheGreeter();
    void lockDrawableSaverNeedsTheGreeter();
    void plainSaverRunsAsItself();
    void startRefusesWhenThereIsNothingToPreview();
    void greeterPreviewStartsAndFinishes();
    void previewsNeverStack();

private:
    FakeCatalog m_catalog;
};

void ScreensaverPreviewTest::kindForNothingChosenIsUnavailable() {
    ProcessScreensaverPreview preview(m_catalog, QStringLiteral("/nonexistent/greeter"));
    QCOMPARE(preview.kindFor(ScreensaverPreferences::noneToken()),
             ScreensaverPreview::Kind::Unavailable);
    QCOMPARE(preview.kindFor(QString()),
             ScreensaverPreview::Kind::Unavailable);
}

void ScreensaverPreviewTest::kindForUnknownTokenIsUnavailable() {
    ProcessScreensaverPreview preview(m_catalog, QStringLiteral("/nonexistent/greeter"));
    // A hand-edited token must never become a program name here either.
    QCOMPARE(preview.kindFor(QStringLiteral("xscreensaver")),
             ScreensaverPreview::Kind::Unavailable);
}

void ScreensaverPreviewTest::blankNeedsTheGreeter() {
    ProcessScreensaverPreview withGreeter(m_catalog, QStringLiteral("/bin/true"));
    QCOMPARE(withGreeter.kindFor(ScreensaverPreferences::blankToken()),
             ScreensaverPreview::Kind::TestingGreeter);

    ProcessScreensaverPreview withoutGreeter(m_catalog, QString());
    QCOMPARE(withoutGreeter.kindFor(ScreensaverPreferences::blankToken()),
             ScreensaverPreview::Kind::Unavailable);
}

void ScreensaverPreviewTest::lockDrawableSaverNeedsTheGreeter() {
    ProcessScreensaverPreview withGreeter(m_catalog, QStringLiteral("/bin/true"));
    QCOMPARE(withGreeter.kindFor(QStringLiteral("qinda-patrol")),
             ScreensaverPreview::Kind::TestingGreeter);

    // Without the greeter there is no way to show the lock-screen rendering,
    // and falling back to the saver program would not be what the user sees
    // when locked.
    ProcessScreensaverPreview withoutGreeter(m_catalog, QString());
    QCOMPARE(withoutGreeter.kindFor(QStringLiteral("qinda-patrol")),
             ScreensaverPreview::Kind::Unavailable);
}

void ScreensaverPreviewTest::plainSaverRunsAsItself() {
    // A saver the greeter cannot draw previews as its own program, exactly
    // what the unlocked idle session shows.
    ProcessScreensaverPreview preview(m_catalog, QStringLiteral("/bin/true"));
    QCOMPARE(preview.kindFor(QStringLiteral("prism-brawl")),
             ScreensaverPreview::Kind::SaverProgram);
    ProcessScreensaverPreview noGreeter(m_catalog, QString());
    QCOMPARE(noGreeter.kindFor(QStringLiteral("prism-brawl")),
             ScreensaverPreview::Kind::SaverProgram);
}

void ScreensaverPreviewTest::startRefusesWhenThereIsNothingToPreview() {
    ProcessScreensaverPreview preview(m_catalog, QStringLiteral("/bin/true"));
    QString error;
    QVERIFY(!preview.start(ScreensaverPreferences::noneToken(), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!preview.running());
}

void ScreensaverPreviewTest::greeterPreviewStartsAndFinishes() {
    // /bin/true stands in for `kscreenlocker_greet --testing`: the point of
    // the row is the process boundary, not the greeter itself (which is
    // exercised by the session rows).
    ProcessScreensaverPreview preview(m_catalog, QStringLiteral("/bin/true"));
    QSignalSpy finishedSpy(&preview, &ScreensaverPreview::finished);
    QString error;
    QVERIFY2(preview.start(ScreensaverPreferences::blankToken(), &error),
             qPrintable(error));
    QTRY_VERIFY(finishedSpy.count() >= 1);
    QVERIFY(!preview.running());
}

void ScreensaverPreviewTest::previewsNeverStack() {
    // The catalog's token names the program; "qinda-slowdouble" is mapped to
    // a real sleeper here so the row holds an in-flight preview and proves a
    // second start is refused instead of stacking windows.
    class SlowCatalog final : public ScreensaverCatalog {
    public:
        [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override
        {
            return {
                {QStringLiteral("sleep"), QStringLiteral("Sleeper"),
                 {}, {}, {QStringLiteral("2")}, false},
            };
        }
    } slowCatalog;

    ProcessScreensaverPreview preview(slowCatalog, QStringLiteral("/bin/true"));
    QString error;
    QVERIFY2(preview.start(QStringLiteral("sleep"), &error), qPrintable(error));
    QVERIFY(preview.running());
    QVERIFY(!preview.start(QStringLiteral("sleep"), &error));
    QVERIFY(!error.isEmpty());
    QTRY_VERIFY(!preview.running());
}

QTEST_MAIN(ScreensaverPreviewTest)
#include "tst_screensaver_preview.moc"
