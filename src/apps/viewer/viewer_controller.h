// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "document_renderer.h"
#include <QObject>
#include <QThread>
#include <QUrl>

namespace QindaQt::Viewer {

// GUI-thread facade. Owns and joins one rendering thread; the QML engine and
// frame provider must be destroyed before this object. No document is modified,
// no remote URL is fetched, and passwords are retained only until close/open.
class ViewerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY stateChanged)
    Q_PROPERTY(int page READ page NOTIFY stateChanged)
    Q_PROPERTY(QSizeF pageSize READ pageSize NOTIFY stateChanged)
    Q_PROPERTY(quint64 frameRevision READ frameRevision NOTIFY frameChanged)
public:
    explicit ViewerController(QObject *parent = nullptr);
    ~ViewerController() override;
    QString fileName() const;
    QString error() const { return m_error; }
    bool busy() const { return m_busy; }
    bool locked() const { return m_locked; }
    bool ready() const { return m_pageCount > 0 && !m_locked && !m_frame.isNull(); }
    int pageCount() const { return m_pageCount; }
    int page() const { return m_page; }
    QSizeF pageSize() const { return m_pageSize; }
    quint64 frameRevision() const { return m_frameRevision; }
    const QImage &frame() const { return m_frame; }
    static QUrl localArgument(const QString &argument);
    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void close();
    Q_INVOKABLE void unlock(const QString &password);
    Q_INVOKABLE void goToPage(int page);
    Q_INVOKABLE void rotate(int degrees);
    Q_INVOKABLE void renderAt(double zoom, double devicePixelRatio);
signals:
    void stateChanged();
    void frameChanged();
    void opened();
    void passwordRequired();
private:
    void requestRender();
    void acceptResult(RenderResult result);
    QThread m_thread;
    QObject *m_worker = nullptr;
    std::shared_ptr<std::atomic<quint64>> m_latest;
    std::shared_ptr<DocumentRenderer> m_renderer;
    RenderRequest m_request;
    QImage m_frame;
    QString m_error;
    QSizeF m_pageSize;
    int m_pageCount = 0;
    int m_page = 0;
    bool m_busy = false;
    bool m_locked = false;
    bool m_opening = false;
    quint64 m_frameRevision = 0;
};
} // namespace QindaQt::Viewer
