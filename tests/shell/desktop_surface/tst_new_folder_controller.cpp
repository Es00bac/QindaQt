// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/new_folder_controller.h"

#include <QDir>
#include <memory>
#include <QFile>
#include <QStandardPaths>
#include <QtTest>

using QindaQt::Shell::DesktopSurface::NewFolderController;

namespace {

// Redirects HOME to a temp root so QStandardPaths::DesktopLocation resolves
// inside the test sandbox; restores the original value afterwards.
class ScopedHomeRedirect {
public:
    explicit ScopedHomeRedirect(const QString &root)
        : m_previous(qEnvironmentVariable("HOME"))
    {
        qputenv("HOME", root.toLocal8Bit());
    }
    ~ScopedHomeRedirect() { qputenv("HOME", m_previous.toLocal8Bit()); }

    Q_DISABLE_COPY(ScopedHomeRedirect)

private:
    QString m_previous;
};

QString desktopPath(const QTemporaryDir &home)
{
    return home.path() + QStringLiteral("/Desktop");
}

} // namespace

class NewFolderControllerTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Per-test temp home: each case starts from a pristine Desktop state.
    void init();
    void cleanup();
    void generatesSequentialUniqueNames();
    void refusesTraversalAndUnsafeNames();
    void publishesFeedbackWhenTheDesktopIsUnavailable();

private:
    std::unique_ptr<QTemporaryDir> m_home;
};

void NewFolderControllerTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
}

void NewFolderControllerTests::cleanup()
{
    m_home.reset();
}

void NewFolderControllerTests::generatesSequentialUniqueNames()
{
    QVERIFY(QDir().mkpath(desktopPath(*m_home)));
    ScopedHomeRedirect redirect(m_home->path());
    NewFolderController controller;

    const QString first = controller.create();
    QCOMPARE(first, desktopPath(*m_home) + QStringLiteral("/New Folder"));
    QVERIFY(QDir(first).exists());
    QCOMPARE(controller.feedback(), QString());

    const QString second = controller.create();
    QCOMPARE(second, desktopPath(*m_home) + QStringLiteral("/New Folder 2"));
    QVERIFY(QDir(second).exists());

    const QString third = controller.create();
    QCOMPARE(third, desktopPath(*m_home) + QStringLiteral("/New Folder 3"));

    // Existing names are never reused, even when the gap is taken.
    QVERIFY(QDir(desktopPath(*m_home)).rmdir(QStringLiteral("New Folder 2")));
    QCOMPARE(controller.create(),
             desktopPath(*m_home) + QStringLiteral("/New Folder 4"));
}

void NewFolderControllerTests::refusesTraversalAndUnsafeNames()
{
    QVERIFY(QDir().mkpath(desktopPath(*m_home)));
    ScopedHomeRedirect redirect(m_home->path());
    NewFolderController controller;

    const QStringList rejected = {
        QStringLiteral("../escape"),
        QStringLiteral(".."),
        QStringLiteral("."),
        QStringLiteral(".hidden"),
        QStringLiteral("a/b"),
        QStringLiteral("back\\slash"),
        QStringLiteral("/etc/passwd"),
    };
    const QStringList before = QDir(desktopPath(*m_home)).entryList(QDir::AllEntries
                                                                   | QDir::Hidden);
    for (const QString &name : rejected) {
        const QString created = controller.create(name);
        QVERIFY2(created.isEmpty(),
                 qPrintable(QStringLiteral("accepted '%1'").arg(name)));
        QVERIFY2(!controller.feedback().isEmpty(),
                 qPrintable(QStringLiteral("no feedback for '%1'").arg(name)));
    }
    const QStringList after = QDir(desktopPath(*m_home)).entryList(QDir::AllEntries
                                                                  | QDir::Hidden);
    QCOMPARE(after, before);

    // A safe explicit name still works after the refusals.
    QCOMPARE(controller.create(QStringLiteral("Meeting notes")),
             desktopPath(*m_home) + QStringLiteral("/Meeting notes"));
    QCOMPARE(controller.feedback(), QString());
}

void NewFolderControllerTests::publishesFeedbackWhenTheDesktopIsUnavailable()
{
    // The Desktop path exists but is a regular file, so every mkdir fails
    // regardless of process privileges.
    QVERIFY(QDir().mkpath(m_home->path()));
    QFile blocker(desktopPath(*m_home));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    ScopedHomeRedirect redirect(m_home->path());
    NewFolderController controller;
    QVERIFY(controller.create().isEmpty());
    QVERIFY(controller.feedback().contains(QStringLiteral("Could not create")));

    controller.clearFeedback();
    QCOMPARE(controller.feedback(), QString());
}

QTEST_GUILESS_MAIN(NewFolderControllerTests)
#include "tst_new_folder_controller.moc"
