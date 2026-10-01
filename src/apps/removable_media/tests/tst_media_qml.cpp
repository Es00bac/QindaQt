// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_appearance.h"
#include "media_controller.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <memory>

using namespace QindaQt::Apps::RemovableMedia;

namespace {
Volume usbVolume() {
    Volume volume;
    volume.token = QStringLiteral("usb-attachment-1");
    volume.device = QStringLiteral("/dev/testusb1");
    volume.label = QStringLiteral("Travel drive");
    volume.kind = QStringLiteral("USB storage");
    volume.size = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    volume.mountable = volume.canMountReadOnly = volume.canFormat = volume.canPowerOff = true;
    volume.identity = QStringLiteral("uuid-usb");
    volume.preferenceKey = QString(64, QLatin1Char('a'));
    return volume;
}

class FixtureBackend final : public MediaBackend {
public:
    QVector<Volume> inventory{usbVolume()};
    QList<Request> requests;
    QVector<Volume> volumes() const override { return inventory; }
    bool available() const override { return true; }
    QString diagnostic() const override { return {}; }
    QStringList formatTypes() const override { return {QStringLiteral("vfat"), QStringLiteral("ext4")}; }
    void refresh() override { Q_EMIT changed(); }
    void execute(const Request &request) override {
        requests.append(request);
        QTimer::singleShot(0, this, [this, request] {
            QString mountPath;
            for (Volume &volume : inventory) {
                if (volume.token != request.token) continue;
                if (request.operation == Operation::Mount || request.operation == Operation::MountReadOnly) {
                    mountPath = QStringLiteral("/run/media/test/Travel drive");
                    volume.mountPath = mountPath;
                } else if (request.operation == Operation::Unmount) volume.mountPath.clear();
            }
            Q_EMIT changed();
            Q_EMIT finished(request.token, true, QStringLiteral("Done"), mountPath);
        });
    }
};

struct Harness final {
    QTemporaryDir directory;
    FixtureBackend backend;
    MediaPreferences preferences{directory.filePath(QStringLiteral("preferences.json"))};
    MediaController controller{backend, preferences, false};
    QQmlApplicationEngine engine;
    std::unique_ptr<QObject> root;
    QQuickWindow *window = nullptr;
    QString error;
    QStringList warnings;

    ~Harness() {
        QObject::disconnect(&engine, nullptr, &engine, nullptr);
        root.reset();
    }

    bool create(bool watch = false) {
        engine.addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
        QObject::connect(&engine, &QQmlEngine::warnings, &engine, [this](const QList<QQmlError> &errors) {
            for (const QQmlError &warning : errors) warnings.append(warning.toString());
        });
        auto *facade = ensureMediaTokenFacade(engine, &error);
        if (facade == nullptr) return false;
        const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
        if (!loaded.ok) { error = loaded.error; return false; }
        if (!facade->publish(loaded.theme, {}, &error)) return false;
        engine.rootContext()->setContextProperty(QStringLiteral("mediaController"), &controller);
        engine.rootContext()->setContextProperty(QStringLiteral("mediaWatchMode"), watch);
        engine.rootContext()->setContextProperty(QStringLiteral("mediaLaunchError"), QString());
        if (!backend.inventory.isEmpty()) controller.select(backend.inventory.constFirst().token);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(QINDAQT_MEDIA_QML_PATH)));
        if (!component.isReady()) { error = component.errorString(); return false; }
        root.reset(component.create());
        if (!root) { error = component.errorString(); return false; }
        window = qobject_cast<QQuickWindow *>(root.get());
        QCoreApplication::processEvents();
        return window != nullptr;
    }
    QObject *control(const char *name) { return root->findChild<QObject *>(QString::fromLatin1(name)); }
    bool click(const char *name) {
        QObject *button = control(name);
        return button && QMetaObject::invokeMethod(button, "click", Qt::DirectConnection);
    }
};
} // namespace

class MediaQmlTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void readOnlyMountOpensAndCanUnmount();
    void remembersFutureInsertionChoice();
    void formatRequiresExactTargetAndUnmount();
    void capturedFormatTargetSurvivesSelectionChange();
    void unplugCancelsFormat();
    void encryptedPassphraseIsSubmittedAndCleared();
    void opticalDiscOffersEjectWithoutFormat();
    void mediaWithoutPowerOffStillOffersSafeRemoval();
    void watcherWindowCanReopenAfterClose();
    void preferenceSurfaceAndFormatButtonDoNotOverlap();
};

void MediaQmlTest::readOnlyMountOpensAndCanUnmount() {
    Harness harness;
    QVERIFY2(harness.create(), qPrintable(harness.error));
    QSignalSpy opened(&harness.controller, &MediaController::openPathRequested);
    QVERIFY(harness.click("mountReadOnlyButton"));
    QTRY_COMPARE(harness.backend.requests.size(), 1);
    QCOMPARE(harness.backend.requests.constFirst().operation, Operation::MountReadOnly);
    QTRY_COMPARE(opened.size(), 1);
    QCOMPARE(opened.constFirst().constFirst().toString(), QStringLiteral("/run/media/test/Travel drive"));
    QVERIFY(harness.click("unmountMediaButton"));
    QTRY_COMPARE(harness.backend.requests.size(), 2);
    QCOMPARE(harness.backend.requests.constLast().operation, Operation::Unmount);
    QTRY_VERIFY(!harness.controller.busy());
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::remembersFutureInsertionChoice() {
    Harness harness;
    QVERIFY2(harness.create(), qPrintable(harness.error));
    auto *chooser = qobject_cast<QQuickItem *>(harness.control("insertionPreference"));
    QVERIFY(chooser != nullptr);
    chooser->forceActiveFocus();
    QTest::keyClick(harness.window, Qt::Key_Down);
    QTRY_COMPARE(harness.controller.selected().value(QStringLiteral("preference")).toString(), QStringLiteral("mount"));
    QCOMPARE(harness.preferences.mode(QString(64, QLatin1Char('a'))), QStringLiteral("mount"));
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::formatRequiresExactTargetAndUnmount() {
    Harness harness;
    harness.backend.inventory[0].mountPath = QStringLiteral("/run/media/test/Travel drive");
    harness.backend.refresh();
    QVERIFY2(harness.create(), qPrintable(harness.error));
    QVERIFY(harness.click("formatMediaButton"));
    const QString capture = qEnvironmentVariable("QINDAQT_MEDIA_FORMAT_CAPTURE");
    if (!capture.isEmpty()) {
        QTest::qWait(100);
        QVERIFY(harness.window->grabWindow().save(capture));
    }
    auto *confirm = harness.control("confirmFormatButton");
    auto *typed = harness.control("formatTypedDevice");
    QVERIFY(confirm != nullptr && typed != nullptr);
    typed->setProperty("text", QStringLiteral("/dev/testusb1"));
    QVERIFY(!confirm->property("enabled").toBool());
    QVERIFY(harness.click("confirmFormatButton"));
    QVERIFY(harness.backend.requests.isEmpty());
    QVERIFY(harness.click("unmountFormatTargetButton"));
    QTRY_VERIFY(!harness.controller.busy());
    QTRY_VERIFY(confirm->property("enabled").toBool());
    typed->setProperty("text", QStringLiteral(" /dev/testusb1"));
    QVERIFY(!confirm->property("enabled").toBool());
    typed->setProperty("text", QStringLiteral("/dev/testusb1"));
    harness.control("formatLabel")->setProperty("text", QStringLiteral("TRAVEL"));
    QVERIFY(harness.click("confirmFormatButton"));
    QTRY_COMPARE(harness.backend.requests.size(), 2);
    QCOMPARE(harness.backend.requests.constLast().operation, Operation::Format);
    QCOMPARE(harness.backend.requests.constLast().token, QStringLiteral("usb-attachment-1"));
    QCOMPARE(harness.backend.requests.constLast().filesystem, QStringLiteral("vfat"));
    QCOMPARE(harness.backend.requests.constLast().label, QStringLiteral("TRAVEL"));
    QCOMPARE(typed->property("text").toString(), QString());
    QTRY_VERIFY(!harness.controller.busy());
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::capturedFormatTargetSurvivesSelectionChange() {
    Harness harness;
    Volume other = usbVolume();
    other.token = QStringLiteral("usb-attachment-2");
    other.device = QStringLiteral("/dev/testusb2");
    other.preferenceKey = QString(64, QLatin1Char('b'));
    harness.backend.inventory.append(other);
    harness.backend.refresh();
    QVERIFY2(harness.create(), qPrintable(harness.error));
    QVERIFY(harness.click("formatMediaButton"));
    harness.controller.select(other.token);
    harness.control("formatTypedDevice")->setProperty("text", QStringLiteral("/dev/testusb1"));
    QVERIFY(harness.click("confirmFormatButton"));
    QTRY_COMPARE(harness.backend.requests.size(), 1);
    QCOMPARE(harness.backend.requests.constFirst().token, QStringLiteral("usb-attachment-1"));
    QTRY_VERIFY(!harness.controller.busy());
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::unplugCancelsFormat() {
    Harness harness;
    QVERIFY2(harness.create(), qPrintable(harness.error));
    QVERIFY(harness.click("formatMediaButton"));
    harness.control("formatTypedDevice")->setProperty("text", QStringLiteral("/dev/testusb1"));
    harness.backend.inventory.clear();
    harness.backend.refresh();
    QCoreApplication::processEvents();
    QVERIFY(harness.controller.formatTarget().isEmpty());
    QVERIFY(!harness.control("confirmFormatButton")->property("enabled").toBool());
    QVERIFY(harness.backend.requests.isEmpty());
    QCOMPARE(harness.control("formatTypedDevice")->property("text").toString(), QString());
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::encryptedPassphraseIsSubmittedAndCleared() {
    Harness harness;
    harness.backend.inventory[0].encrypted = true;
    harness.backend.inventory[0].mountable = false;
    harness.backend.inventory[0].canMountReadOnly = false;
    harness.backend.refresh();
    QVERIFY2(harness.create(), qPrintable(harness.error));
    auto *passphrase = qobject_cast<QQuickItem *>(harness.control("mediaPassphrase"));
    QVERIFY(passphrase != nullptr);
    passphrase->setProperty("text", QStringLiteral("fixture-passphrase"));
    passphrase->forceActiveFocus();
    QTest::keyClick(harness.window, Qt::Key_Return);
    QTRY_COMPARE(harness.backend.requests.size(), 1);
    QCOMPARE(harness.backend.requests.constFirst().operation, Operation::Unlock);
    QCOMPARE(harness.backend.requests.constFirst().passphrase, QStringLiteral("fixture-passphrase"));
    QCOMPARE(passphrase->property("text").toString(), QString());
    QTRY_VERIFY(!harness.controller.busy());
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::opticalDiscOffersEjectWithoutFormat() {
    Harness harness;
    auto &disc = harness.backend.inventory[0];
    disc.kind = QStringLiteral("Blu-ray disc");
    disc.label = QStringLiteral("Film archive");
    disc.optical = disc.readOnly = disc.canEject = true;
    disc.canPowerOff = disc.canFormat = false;
    harness.backend.refresh();
    QVERIFY2(harness.create(), qPrintable(harness.error));
    QVERIFY(!harness.control("formatMediaButton")->property("visible").toBool());
    QCOMPARE(harness.control("removeMediaButton")->property("text").toString(), QStringLiteral("Eject disc"));
    QVERIFY(harness.click("removeMediaButton"));
    QTRY_COMPARE(harness.backend.requests.size(), 1);
    QCOMPARE(harness.backend.requests.constFirst().operation, Operation::Remove);
    QTRY_VERIFY(!harness.controller.busy());
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::watcherWindowCanReopenAfterClose() {
    Harness harness;
    QVERIFY2(harness.create(true), qPrintable(harness.error));
    QVERIFY(!harness.window->isVisible());
    harness.controller.show();
    QTRY_VERIFY(harness.window->isVisible());
    harness.window->close();
    QVERIFY(!harness.window->isVisible());
    harness.controller.show();
    QTRY_VERIFY(harness.window->isVisible());
    const QString capture = qEnvironmentVariable("QINDAQT_MEDIA_CAPTURE");
    if (!capture.isEmpty()) {
        QTest::qWait(250);
        QVERIFY(harness.window->grabWindow().save(capture));
    }
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::mediaWithoutPowerOffStillOffersSafeRemoval() {
    Harness harness;
    harness.backend.inventory[0].canPowerOff = false;
    harness.backend.inventory[0].mountPath = QStringLiteral("/run/media/test/Travel drive");
    harness.backend.refresh();
    QVERIFY2(harness.create(), qPrintable(harness.error));
    auto *button = harness.control("removeMediaButton");
    QVERIFY(button != nullptr && button->property("visible").toBool());
    QCOMPARE(button->property("text").toString(), QStringLiteral("Safely remove"));
    QVERIFY(harness.click("removeMediaButton"));
    QTRY_COMPARE(harness.backend.requests.size(), 1);
    QCOMPARE(harness.backend.requests.constFirst().operation, Operation::Remove);
    QTRY_VERIFY(!harness.controller.busy());
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

void MediaQmlTest::preferenceSurfaceAndFormatButtonDoNotOverlap() {
    Harness harness;
    QVERIFY2(harness.create(), qPrintable(harness.error));
    auto *surface = qobject_cast<QQuickItem *>(harness.control("insertionPreferenceSurface"));
    auto *button = qobject_cast<QQuickItem *>(harness.control("formatMediaButton"));
    auto *chooser = qobject_cast<QQuickItem *>(harness.control("insertionPreference"));
    QVERIFY(surface != nullptr && button != nullptr && chooser != nullptr);
    for (const QSize size : {QSize(760, 720), QSize(560, 540)}) {
        harness.window->resize(size);
        QTest::qWait(25);
        QVERIFY(surface->height() > chooser->height());
        const qreal surfaceBottom = surface->mapToScene(QPointF(0, surface->height())).y();
        const qreal buttonTop = button->mapToScene(QPointF()).y();
        QVERIFY2(buttonTop >= surfaceBottom, "Format overlaps the future-insertion preference form");
        QVERIFY(chooser->width() >= 300);
    }
    QVERIFY2(harness.warnings.isEmpty(), qPrintable(harness.warnings.join('\n')));
}

QTEST_MAIN(MediaQmlTest)
#include "tst_media_qml.moc"
