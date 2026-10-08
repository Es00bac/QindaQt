// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "private_bus.h"

#include <qindaqt/compositor_names/compositor_names.h>

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtDBus/QDBusContext>
#include <QtDBus/QDBusMessage>

#include <cstring>
#include <memory>
#include <utility>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace QindaQt::Tests {
inline QString compositorService() { return QString(CompositorNames::service); }
inline QString nativeLockPath() { return QString(CompositorNames::nativeLockPath); }
inline QString nativeLockInterface() { return QString(CompositorNames::nativeLockInterface); }

// Actual kernel socket peer and daemon owner/PID proof. These ordinary dummy
// endpoints are private-bus fixtures, never installed or connected to a display.
class ClipboardOrdinaryListener final {
public:
    explicit ClipboardOrdinaryListener(QString path) : pathname(std::move(path)) {
        fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) return;
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        const auto bytes = QFile::encodeName(pathname);
        if (bytes.size() >= qsizetype(sizeof(address.sun_path))) return;
        std::memcpy(address.sun_path, bytes.constData(), size_t(bytes.size()) + 1);
        ready = bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0
            && listen(fd, 16) == 0
            && QFile::setPermissions(pathname, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    ~ClipboardOrdinaryListener() {
        if (fd >= 0) close(fd);
        QFile::remove(pathname);
    }
    QString pathname;
    int fd = -1;
    bool ready = false;
};

class ClipboardNativeBackend final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.KWin.NativeLock1")
public:
    ClipboardNativeBackend(QDBusConnection connection, QDBusConnection hostile)
        : bus(std::move(connection)), attacker(std::move(hostile)) {}
    bool expose(bool object = true) {
        return (!object || exposeObject()) && bus.registerService(compositorService());
    }
    bool exposeObject() {
        return bus.registerObject(nativeLockPath(), this,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
    }
    void setState(bool nextLocked, bool nextProtected) {
        locked = nextLocked;
        protectedPresentation = nextProtected;
        Q_EMIT lockedChanged(locked);
        Q_EMIT protectedChanged(protectedPresentation);
    }
    QString receiptMode = QStringLiteral("valid");
    int requests = 0;
public Q_SLOTS:
    void RequestStateWithReceipt(const QString &nonce) {
        ++requests;
        if (receiptMode == QStringLiteral("missing")) return;
        auto receipt = QDBusMessage::createTargetedSignal(message().service(),
            nativeLockPath(), nativeLockInterface(), QStringLiteral("stateReceipt"));
        receipt << (receiptMode == QStringLiteral("wrong-nonce")
                        ? QStringLiteral("00000000000000000000000000000000") : nonce)
                << locked << protectedPresentation;
        auto &sender = receiptMode == QStringLiteral("forged-owner") ? attacker : bus;
        sender.send(receipt);
        if (receiptMode == QStringLiteral("duplicate")) bus.send(receipt);
    }
Q_SIGNALS:
    void lockedChanged(bool locked);
    void protectedChanged(bool protectedPresentation);
private:
    QDBusConnection bus, attacker;
    bool locked = false, protectedPresentation = false;
};

class NativeClipboardFixture final {
public:
    bool start(bool object = true, bool sessionOwner = true) {
        if (!privateBus.start() || !runtime.isValid()) return false;
        compositor = privateBus.connectClient(QStringLiteral("compositor"));
        session = privateBus.connectClient(QStringLiteral("session"));
        client = privateBus.connectClient(QStringLiteral("observer"));
        attacker = privateBus.connectClient(QStringLiteral("attacker"));
        if ((sessionOwner && !session.registerService(QStringLiteral("org.qindaqt.Session1")))
            || !session.registerService(QStringLiteral("org.freedesktop.ScreenSaver"))
            || !session.registerService(QStringLiteral("org.kde.screensaver"))) return false;
        backend = std::make_unique<ClipboardNativeBackend>(compositor, attacker);
        if (!backend->expose(object)) return false;
        listener = std::make_unique<ClipboardOrdinaryListener>(runtime.filePath(basename));
        return listener->ready;
    }
    PrivateClipboardBus privateBus;
    QDBusConnection compositor{QStringLiteral("invalid")};
    QDBusConnection session{QStringLiteral("invalid")};
    QDBusConnection client{QStringLiteral("invalid")};
    QDBusConnection attacker{QStringLiteral("invalid")};
    QTemporaryDir runtime;
    QString basename = QString(CompositorNames::waylandSocketPrefix) + QStringLiteral("0");
    std::unique_ptr<ClipboardOrdinaryListener> listener;
    std::unique_ptr<ClipboardNativeBackend> backend;
};
} // namespace QindaQt::Tests
