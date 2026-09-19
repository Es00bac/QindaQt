// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_power/lock_screen_saver_store.h>

#include <KConfig>
#include <KConfigGroup>

#include <QtTest>

using QindaQt::Apps::SettingsPower::KConfigLockScreenSaverStore;

namespace {

constexpr auto kGreeter = "Greeter";
constexpr auto kWallpaperPlugin = "WallpaperPlugin";

[[nodiscard]] QString readWallpaperPlugin(const QString &path)
{
    KConfig config(path, KConfig::SimpleConfig);
    return config.group(QString::fromLatin1(kGreeter))
        .readEntry(QString::fromLatin1(kWallpaperPlugin), QString{});
}

[[nodiscard]] KConfigGroup pluginGeneral(KConfig &config)
{
    return config.group(QString::fromLatin1(kGreeter))
        .group(QStringLiteral("Wallpaper"))
        .group(KConfigLockScreenSaverStore::wallpaperPluginId())
        .group(QStringLiteral("General"));
}

} // namespace

class LockScreenSaverStoreTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void choosingASaverTakesOverTheGreeterWallpaper();
    void theDisplacedPluginIsRememberedAndGivenBack();
    void takingOverTwiceKeepsTheOriginalPredecessor();
    void noPredecessorLeavesTheKeyAbsent();
    void unrelatedLockerKeysSurvive();
    void anotherPluginMeansNoSaverOnTheLockScreen();

private:
    [[nodiscard]] QString path() const { return m_directory.filePath(QStringLiteral("kscreenlockerrc")); }

    QTemporaryDir m_directory;
};

void LockScreenSaverStoreTest::init() {
    QVERIFY(m_directory.isValid());
    QFile::remove(path());
}

void LockScreenSaverStoreTest::choosingASaverTakesOverTheGreeterWallpaper() {
    KConfigLockScreenSaverStore store(path());
    QString error;
    QVERIFY2(store.save(QStringLiteral("qinda-patrol"), &error), qPrintable(error));

    QCOMPARE(readWallpaperPlugin(path()),
             KConfigLockScreenSaverStore::wallpaperPluginId());
    KConfig config(path(), KConfig::SimpleConfig);
    QCOMPARE(pluginGeneral(config).readEntry(QStringLiteral("Saver"), QString{}),
             QStringLiteral("qinda-patrol"));
    QCOMPARE(store.currentSaver(), QStringLiteral("qinda-patrol"));
}

void LockScreenSaverStoreTest::theDisplacedPluginIsRememberedAndGivenBack() {
    {
        KConfig config(path(), KConfig::SimpleConfig);
        config.group(QString::fromLatin1(kGreeter))
            .writeEntry(QString::fromLatin1(kWallpaperPlugin), QStringLiteral("org.kde.image"));
        QVERIFY(config.sync());
    }

    KConfigLockScreenSaverStore store(path());
    QString error;
    QVERIFY2(store.save(QStringLiteral("circuit-reef"), &error), qPrintable(error));
    QCOMPARE(readWallpaperPlugin(path()),
             KConfigLockScreenSaverStore::wallpaperPluginId());

    // Turning the saver off must hand the user their own lock wallpaper back,
    // not leave them staring at this plugin's plain ground.
    QVERIFY2(store.save(QStringLiteral("none"), &error), qPrintable(error));
    QCOMPARE(readWallpaperPlugin(path()), QStringLiteral("org.kde.image"));
    QCOMPARE(store.currentSaver(), QStringLiteral("none"));
}

void LockScreenSaverStoreTest::takingOverTwiceKeepsTheOriginalPredecessor() {
    {
        KConfig config(path(), KConfig::SimpleConfig);
        config.group(QString::fromLatin1(kGreeter))
            .writeEntry(QString::fromLatin1(kWallpaperPlugin), QStringLiteral("org.kde.slideshow"));
        QVERIFY(config.sync());
    }

    KConfigLockScreenSaverStore store(path());
    QString error;
    QVERIFY(store.save(QStringLiteral("qinda-patrol"), &error));
    // AGENT-GUARD: a second take-over must not record our own id as the
    // predecessor; that would make "none" restore the screensaver plugin and
    // lose the user's wallpaper for good.
    QVERIFY(store.save(QStringLiteral("circuit-reef"), &error));
    QVERIFY(store.save(QStringLiteral("none"), &error));
    QCOMPARE(readWallpaperPlugin(path()), QStringLiteral("org.kde.slideshow"));
}

void LockScreenSaverStoreTest::noPredecessorLeavesTheKeyAbsent() {
    KConfigLockScreenSaverStore store(path());
    QString error;
    QVERIFY(store.save(QStringLiteral("qinda-patrol"), &error));
    QVERIFY(store.save(QStringLiteral("none"), &error));
    // No recorded predecessor: the greeter falls back to its own default
    // rather than to a plugin this store invented.
    QVERIFY(readWallpaperPlugin(path()).isEmpty());
}

void LockScreenSaverStoreTest::unrelatedLockerKeysSurvive() {
    {
        KConfig config(path(), KConfig::SimpleConfig);
        KConfigGroup daemon = config.group(QStringLiteral("Daemon"));
        daemon.writeEntry(QStringLiteral("Autolock"), true);
        daemon.writeEntry(QStringLiteral("Timeout"), 7);
        QVERIFY(config.sync());
    }

    KConfigLockScreenSaverStore store(path());
    QString error;
    QVERIFY(store.save(QStringLiteral("qinda-patrol"), &error));

    // AGENT-GUARD: automatic locking belongs to the screen-lock section's own
    // store. This one may never move it.
    KConfig config(path(), KConfig::SimpleConfig);
    const KConfigGroup daemon = config.group(QStringLiteral("Daemon"));
    QCOMPARE(daemon.readEntry(QStringLiteral("Autolock"), false), true);
    QCOMPARE(daemon.readEntry(QStringLiteral("Timeout"), 0), 7);
}

void LockScreenSaverStoreTest::anotherPluginMeansNoSaverOnTheLockScreen() {
    KConfigLockScreenSaverStore store(path());
    QString error;
    QVERIFY(store.save(QStringLiteral("qinda-patrol"), &error));
    {
        KConfig config(path(), KConfig::SimpleConfig);
        config.group(QString::fromLatin1(kGreeter))
            .writeEntry(QString::fromLatin1(kWallpaperPlugin), QStringLiteral("org.kde.potd"));
        QVERIFY(config.sync());
    }
    // Someone else owns the lock wallpaper now, so no saver is on that screen
    // whatever this plugin's own group still says.
    QCOMPARE(store.currentSaver(), QStringLiteral("none"));
}

QTEST_MAIN(LockScreenSaverStoreTest)
#include "tst_lock_screen_saver_store.moc"
