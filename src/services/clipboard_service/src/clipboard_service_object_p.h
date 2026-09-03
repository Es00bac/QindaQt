// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_service/clipboard_host.h>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtDBus/QDBusContext>

namespace QindaQt::Services::Clipboard {

class ClipboardServiceObject final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Clipboard1")
    Q_CLASSINFO("D-Bus Introspection",
        "<interface name=\"org.qindaqt.Clipboard1\">"
        "<method name=\"GetSnapshot\"><arg name=\"snapshot\" type=\"(ututbbay)\" direction=\"out\"/></method>"
        "<method name=\"Select\"><arg name=\"requestId\" type=\"t\" direction=\"in\"/><arg name=\"epoch\" type=\"t\" direction=\"in\"/><arg name=\"generation\" type=\"u\" direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/><arg name=\"entry\" type=\"(uu)\" direction=\"in\"/><arg name=\"result\" type=\"(uuttuttuts)\" direction=\"out\"/></method>"
        "<method name=\"Delete\"><arg name=\"requestId\" type=\"t\" direction=\"in\"/><arg name=\"epoch\" type=\"t\" direction=\"in\"/><arg name=\"generation\" type=\"u\" direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/><arg name=\"entry\" type=\"(uu)\" direction=\"in\"/><arg name=\"result\" type=\"(uuttuttuts)\" direction=\"out\"/></method>"
        "<method name=\"Clear\"><arg name=\"requestId\" type=\"t\" direction=\"in\"/><arg name=\"epoch\" type=\"t\" direction=\"in\"/><arg name=\"generation\" type=\"u\" direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/><arg name=\"all\" type=\"b\" direction=\"in\"/><arg name=\"result\" type=\"(uuttuttuts)\" direction=\"out\"/></method>"
        "<method name=\"Copy\"><arg name=\"requestId\" type=\"t\" direction=\"in\"/><arg name=\"epoch\" type=\"t\" direction=\"in\"/><arg name=\"generation\" type=\"u\" direction=\"in\"/><arg name=\"revision\" type=\"t\" direction=\"in\"/><arg name=\"entry\" type=\"(uu)\" direction=\"in\"/><arg name=\"result\" type=\"(uuttuttuts)\" direction=\"out\"/></method>"
        "<signal name=\"Changed\"><arg name=\"epoch\" type=\"t\"/><arg name=\"generation\" type=\"u\"/><arg name=\"revision\" type=\"t\"/></signal>"
        "</interface>")
public:
    explicit ClipboardServiceObject(ClipboardHost *host, QObject *parent = nullptr);
    void forgetCaller(const QString &caller);

public Q_SLOTS:
    Q_SCRIPTABLE Snapshot GetSnapshot() const;
    Q_SCRIPTABLE OperationResult Select(quint64 requestId, quint64 epoch,
                                        quint32 generation, quint64 revision,
                                        const ClipboardModel::EntryId &entry);
    Q_SCRIPTABLE OperationResult Delete(quint64 requestId, quint64 epoch,
                                        quint32 generation, quint64 revision,
                                        const ClipboardModel::EntryId &entry);
    Q_SCRIPTABLE OperationResult Clear(quint64 requestId, quint64 epoch,
                                       quint32 generation, quint64 revision, bool all);
    Q_SCRIPTABLE OperationResult Copy(quint64 requestId, quint64 epoch,
                                      quint32 generation, quint64 revision,
                                      const ClipboardModel::EntryId &entry);

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(quint64 epoch, quint32 generation, quint64 revision);

private:
    struct Remembered { OperationRequest request; OperationResult result; };
    struct CallerRequests {
        QHash<quint64, Remembered> byId;
        QList<quint64> oldestFirst;
    };
    OperationResult submit(OperationRequest request);
    ClipboardHost *m_host = nullptr;
    QHash<QString, CallerRequests> m_remembered;
};

} // namespace QindaQt::Services::Clipboard
