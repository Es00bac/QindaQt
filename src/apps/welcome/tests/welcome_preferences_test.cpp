// SPDX-License-Identifier: GPL-3.0-or-later
#include "../welcome_preferences.h"

#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class WelcomePreferencesTest final : public QObject {
    Q_OBJECT
private slots:
    void defaultsToShowingAndPersistsOptOut() {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                           directory.path());
        QSettings().clear();
        QindaQt::Apps::Welcome::WelcomePreferences preferences;
        QVERIFY(preferences.showAtNextLaunch());
        preferences.setShowAtNextLaunch(false);
        QindaQt::Apps::Welcome::WelcomePreferences reopened;
        QVERIFY(!reopened.showAtNextLaunch());
    }
};
QTEST_GUILESS_MAIN(WelcomePreferencesTest)
#include "welcome_preferences_test.moc"
