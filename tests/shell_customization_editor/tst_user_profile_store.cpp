// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_test_fixtures.h"

#include "qindaqt/profiles/profile_loader.h"
#include "qindaqt/shell_customization_editor/user_profile_store.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace QindaQt;
using namespace QindaQt::Profiles;
using namespace QindaQt::ShellCustomizationEditor;
using namespace QindaQt::ShellCustomizationEditor::TestFixtures;

class UserProfileStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void savedProfilesRoundTripThroughTheStrictLoader() const
    {
        // Restart safety: the bytes on disk must be a complete, strict
        // schema-v1 document that the shell's own loader accepts.
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const UserProfileStore store{directory.path()};
        const Profiles::LayoutProfile source = profile();
        const ProfileStoreResult result = store.save(source);

        QVERIFY(result.ok);
        QCOMPARE(result.path, directory.path() + QStringLiteral("/editor-fixture.json"));

        const auto loaded =
            QindaQt::Profiles::ProfileLoader::fromJson(QFile(result.path).readAll(),
                                                       QStringLiteral("user-store-test"));
        QVERIFY(loaded.ok);
        QCOMPARE(loaded.profile.id, QStringLiteral("editor-fixture"));
        QCOMPARE(loaded.profile.panels.size(), source.panels.size());
    }

    void savingAgainReplacesThePreviousDocumentAtomically() const
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const UserProfileStore store{directory.path()};
        Profiles::LayoutProfile source = profile();
        QVERIFY(store.save(source).ok);

        source.panels.clear();
        QVERIFY(store.save(source).ok);

        const auto loaded = QindaQt::Profiles::ProfileLoader::fromFile(
            directory.path() + QStringLiteral("/editor-fixture.json"));
        QVERIFY(loaded.ok);
        QCOMPARE(loaded.profile.panels.size(), 0);
    }

    void identifiersThatCannotBeFileNamesAreRejected() const
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const UserProfileStore store{directory.path()};

        for (const QString &identifier : {QStringLiteral("../escape"),
                                          QStringLiteral("a/b"),
                                          QStringLiteral(".hidden"),
                                          QStringLiteral(".."),
                                          QString(),
                                          QStringLiteral("id with space")}) {
            Profiles::LayoutProfile source = profile();
            source.id = identifier;
            const ProfileStoreResult result = store.save(source);
            QVERIFY(!result.ok);
            QCOMPARE(result.code, ProfileStoreErrorCode::InvalidProfileId);
        }
        QVERIFY(QDir(directory.path()).isEmpty());
    }

    void aMissingDirectoryIsCreatedAndAnUnavailableOneIsTyped() const
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString nested = directory.path() + QStringLiteral("/a/b/c");
        const UserProfileStore creating{nested};
        const ProfileStoreResult created = creating.save(profile());
        QVERIFY(created.ok);

        // A regular file in place of the directory fails deterministically
        // regardless of privileges.
        QFile blocker{directory.path() + QStringLiteral("/asdir")};
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();

        const UserProfileStore blocked{blocker.fileName()};
        const ProfileStoreResult failure = blocked.save(profile());
        QVERIFY(!failure.ok);
        QCOMPARE(failure.code, ProfileStoreErrorCode::DirectoryUnavailable);
    }

    void emptyIdsAreRejectedBeforeAnyIO() const
    {
        const UserProfileStore store{QStringLiteral("/unused")};
        QVERIFY(!UserProfileStore::isValidProfileId(QString()));
        QVERIFY(UserProfileStore::isValidProfileId(QStringLiteral("my-dock_2.json-ish")));
    }
};

QTEST_MAIN(UserProfileStoreTest)
#include "tst_user_profile_store.moc"
