// SPDX-License-Identifier: GPL-3.0-or-later
#include "clipboard_publisher.h"

#include <QBuffer>
#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMimeData>

namespace QindaQt::Screenshot {
namespace {

using Services::ClipboardModel::ClipboardFormat;
using Services::ClipboardModel::ClipboardValue;
using Services::ClipboardWayland::SelectionKind;
using Services::ClipboardWayland::StartStatus;

// The data-control device appears after one registry round trip.
constexpr int AvailabilityDeadlineMilliseconds = 1500;
constexpr int LeaseWatchMilliseconds = 1000;

} // namespace

ClipboardPublisher::ClipboardPublisher(AdapterFactory factory, QObject *parent)
    : QObject(parent)
    , m_factory(std::move(factory))
{
    m_availabilityDeadline.setSingleShot(true);
    m_availabilityDeadline.setInterval(AvailabilityDeadlineMilliseconds);
    connect(&m_availabilityDeadline, &QTimer::timeout, this, [this] {
        m_adapterFailed = true;
        fallBack();
    });
    m_leaseWatch.setInterval(LeaseWatchMilliseconds);
    connect(&m_leaseWatch, &QTimer::timeout, this, [this] {
        if (!m_adapter || !m_adapter->publishedSelectionLive()) {
            m_leaseWatch.stop();
            setHolding(false);
        }
    });
}

ClipboardPublisher::~ClipboardPublisher()
{
    if (m_adapter) {
        m_adapter->setObserver(nullptr);
        m_adapter->stop();
    }
}

void ClipboardPublisher::copyImage(const QImage &image)
{
    if (image.isNull()) {
        Q_EMIT failed(tr("There is no screenshot to copy."));
        return;
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        Q_EMIT failed(tr("The screenshot could not be encoded for the clipboard."));
        return;
    }
    publish(ClipboardValue{{ClipboardFormat{QStringLiteral("image/png"), png}}});
}

void ClipboardPublisher::copyText(const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    publish(ClipboardValue{{ClipboardFormat{QStringLiteral("text/plain;charset=utf-8"), utf8},
                            ClipboardFormat{QStringLiteral("text/plain"), utf8}}});
}

void ClipboardPublisher::publish(ClipboardValue value)
{
    m_pending = std::move(value);
    if (!m_adapter && !m_adapterFailed && m_factory) {
        m_adapter = m_factory();
        if (m_adapter) {
            m_adapter->setObserver(this);
            m_adapter->setCaptureEnabled(false);
            const StartStatus status = m_adapter->start();
            if (status != StartStatus::Started && status != StartStatus::AlreadyStarted) {
                m_adapter.reset();
                m_adapterFailed = true;
            }
        } else {
            m_adapterFailed = true;
        }
    }
    if (!m_adapter) {
        fallBack();
        return;
    }
    if (m_adapter->isAvailable()) {
        publishPending();
        return;
    }
    m_availabilityDeadline.start();
}

void ClipboardPublisher::captureAvailabilityChanged(bool available)
{
    if (available && m_pending) {
        m_availabilityDeadline.stop();
        publishPending();
    } else if (!available && m_holding) {
        m_leaseWatch.stop();
        setHolding(false);
    }
}

void ClipboardPublisher::publishPending()
{
    if (!m_pending || !m_adapter)
        return;
    const ClipboardValue value = *m_pending;
    if (!m_adapter->publishSelection(SelectionKind::Clipboard, value)) {
        fallBack();
        return;
    }
    m_pending.reset();
    setHolding(true);
    m_leaseWatch.start();
    Q_EMIT copied(true);
}

void ClipboardPublisher::fallBack()
{
    if (!m_pending)
        return;
    const ClipboardValue value = *m_pending;
    m_pending.reset();
    auto *clipboard = QGuiApplication::clipboard();
    if (!clipboard) {
        Q_EMIT failed(tr("The clipboard is not available."));
        return;
    }
    auto *mime = new QMimeData;
    for (const ClipboardFormat &format : value.formats) {
        if (format.mediaType == QLatin1String("image/png"))
            mime->setImageData(QImage::fromData(format.payload, "PNG"));
        else if (format.mediaType == QLatin1String("text/plain"))
            mime->setText(QString::fromUtf8(format.payload));
    }
    // AGENT-NOTE: without data control this only succeeds while one of our
    // windows has keyboard focus (the result window's Copy button).
    clipboard->setMimeData(mime);
    Q_EMIT copied(false);
}

void ClipboardPublisher::setHolding(bool holding)
{
    if (m_holding == holding)
        return;
    m_holding = holding;
    Q_EMIT holdsSelectionChanged();
}

} // namespace QindaQt::Screenshot
