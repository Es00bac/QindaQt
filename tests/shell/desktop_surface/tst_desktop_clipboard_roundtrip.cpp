// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_surface/desktop_contents_controller.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using QindaQt::Shell::DesktopSurface::DesktopContentsController;

namespace {

[[nodiscard]] bool writeFile(const QString &path, const QByteArray &contents = {})
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(contents);
    return true;
}

[[nodiscard]] QVariantMap rowNamed(const QVariantList &rows, const QString &label)
{
    for (const QVariant &row : rows) {
        const QVariantMap map = row.toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            return map;
        }
    }
    return {};
}

} // namespace

// Desktop clipboard composition proves end to end under the offscreen
// platform: a foreign application's uri-list is adopted copy-only and pastes
// into the Desktop directory through the same identity-checked local
// authority File Manager uses; the Desktop's own cut reports its mode and a
// paste back into the same folder refuses as a no-op instead of moving
// anything. HOME and XDG_DATA_HOME are redirected, so nothing touches the
// user's real directories or clipboard expectations beyond the process.
class DesktopClipboardRoundtripTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void adoptsForeignClipboardAndPastesIntoTheDesktop();
    void cutSelectionReportsModeAndRefusesAPasteIntoItself();

private:
    std::unique_ptr<QTemporaryDir> m_home;
    std::unique_ptr<QTemporaryDir> m_outside;
    QByteArray m_previousHome;
    QByteArray m_previousDataHome;
};

void DesktopClipboardRoundtripTests::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    m_outside = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid() && m_outside->isValid());
    m_previousHome = qgetenv("HOME");
    m_previousDataHome = qgetenv("XDG_DATA_HOME");
    qputenv("HOME", m_home->path().toLocal8Bit());
    qputenv("XDG_DATA_HOME",
            (m_home->path() + QStringLiteral("/.local/share")).toLocal8Bit());
    QVERIFY(QDir().mkpath(m_home->path() + QStringLiteral("/Desktop")));
}

void DesktopClipboardRoundtripTests::cleanup()
{
    QGuiApplication::clipboard()->clear();
    qputenv("HOME", m_previousHome);
    qputenv("XDG_DATA_HOME", m_previousDataHome);
    m_home.reset();
    m_outside.reset();
}

void DesktopClipboardRoundtripTests::
    adoptsForeignClipboardAndPastesIntoTheDesktop()
{
    const QString clipping = m_outside->filePath(QStringLiteral("Clipping.txt"));
    QVERIFY(writeFile(clipping, "portable"));

    // Another application owns the clipboard with a plain uri-list.
    auto *mime = new QMimeData;
    mime->setUrls({QUrl::fromLocalFile(clipping)});
    QGuiApplication::clipboard()->setMimeData(mime);

    DesktopContentsController controller(m_home->path() + QStringLiteral("/Desktop"),
                                         QStringList{}, QGuiApplication::clipboard(),
                                         nullptr);
    QTRY_VERIFY(controller.canPaste());
    QCOMPARE(controller.clipboardMode(), QStringLiteral("copy"));

    QVERIFY2(controller.pasteIntoDesktop(), qPrintable(controller.feedback()));
    const QString pasted =
        m_home->path() + QStringLiteral("/Desktop/Clipping.txt");
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(pasted), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(
        !rowNamed(controller.rows(), QStringLiteral("Clipping.txt")).isEmpty(),
        5000);
    // Foreign adoption is copy-only: the source keeps existing.
    QVERIFY(QFileInfo::exists(clipping));
}

void DesktopClipboardRoundtripTests::
    cutSelectionReportsModeAndRefusesAPasteIntoItself()
{
    const QString first = m_home->path() + QStringLiteral("/Desktop/First.txt");
    const QString second = m_home->path() + QStringLiteral("/Desktop/Second.txt");
    QVERIFY(writeFile(first));
    QVERIFY(writeFile(second));

    DesktopContentsController controller(m_home->path() + QStringLiteral("/Desktop"),
                                         QStringList{}, QGuiApplication::clipboard(),
                                         nullptr);
    QCOMPARE(controller.rows().size(), 2);
    QVERIFY2(controller.cutSelection(controller.rows()),
             qPrintable(controller.feedback()));
    QVERIFY(controller.canPaste());
    QCOMPARE(controller.clipboardMode(), QStringLiteral("cut"));

    // The desktop paste target is the Desktop itself; moving the entries onto
    // their own parent is an already-exists no-op, refused before dispatch.
    QVERIFY(!controller.pasteIntoDesktop());
    QVERIFY(controller.feedback().contains(QStringLiteral("Nothing to paste")));
    QVERIFY(QFileInfo::exists(first));
    QVERIFY(QFileInfo::exists(second));
}

QTEST_MAIN(DesktopClipboardRoundtripTests)
#include "tst_desktop_clipboard_roundtrip.moc"
