// SPDX-License-Identifier: GPL-3.0-or-later
#include "osk_keyboard_model.h"

#include <QVariantMap>
#include <algorithm>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace QindaQt::Apps::Osk {

KeyEmitter::~KeyEmitter() = default;

OskKeyboardModel::OskKeyboardModel(QObject *parent) : QObject(parent) {}

void OskKeyboardModel::setDocument(const OskLayoutDocument &document)
{
    m_document = document;
    m_shifted = false;
    if (m_document.symbols.isEmpty()) {
        m_symbolsPage = false;
    }
    Q_EMIT rowsChanged();
}

void OskKeyboardModel::setMetrics(const Metrics &metrics)
{
    m_metrics = metrics;
    Q_EMIT rowsChanged();
}

void OskKeyboardModel::relayout(qreal width, qreal keyHeight, qreal gap, qreal margin)
{
    Metrics metrics = m_metrics;
    metrics.width = std::max<qreal>(0.0, width);
    metrics.keyHeight = std::max<qreal>(24.0, keyHeight);
    metrics.gap = std::max<qreal>(0.0, gap);
    metrics.margin = std::max<qreal>(0.0, margin);
    setMetrics(metrics);
}

const QList<KeyRow> &OskKeyboardModel::currentPage() const
{
    return m_symbolsPage && !m_document.symbols.isEmpty() ? m_document.symbols : m_document.letters;
}

QString OskKeyboardModel::labelFor(const KeyDefinition &key) const
{
    switch (key.kind) {
    case KeyKind::Text: return m_shifted ? key.shiftedText : key.text;
    case KeyKind::Shift: return QStringLiteral("⇧");
    case KeyKind::Backspace: return QStringLiteral("⌫");
    case KeyKind::Symbols: return QStringLiteral("?123");
    case KeyKind::Letters: return QStringLiteral("ABC");
    case KeyKind::Space: return QString();
    case KeyKind::Enter: return QStringLiteral("⏎");
    case KeyKind::Hide: return QStringLiteral("⌨▾");
    case KeyKind::Layout: return m_document.label;
    }
    return QString();
}

qreal OskKeyboardModel::panelHeight() const
{
    const auto rows = static_cast<qreal>(currentPage().size());
    if (rows <= 0.0) {
        return 0.0;
    }
    return m_metrics.margin * 2 + rows * m_metrics.keyHeight + (rows - 1) * m_metrics.gap;
}

QList<QList<PlacedKey>> OskKeyboardModel::placedRows() const
{
    QList<QList<PlacedKey>> placed;
    const QList<KeyRow> &page = currentPage();
    const qreal available = m_metrics.width - 2 * m_metrics.margin;
    for (qsizetype rowIndex = 0; rowIndex < page.size(); ++rowIndex) {
        const KeyRow &row = page.at(rowIndex);
        qreal units = 0.0;
        for (const KeyDefinition &key : row) {
            units += key.width;
        }
        const qreal gaps = m_metrics.gap * static_cast<qreal>(row.size() - 1);
        const qreal unit = units > 0.0
            ? std::min(m_metrics.maximumUnit, std::max<qreal>(0.0, (available - gaps) / units))
            : 0.0;
        const qreal rowWidth = unit * units + gaps;
        qreal x = m_metrics.margin + std::max<qreal>(0.0, (available - rowWidth) / 2.0);
        const qreal y = m_metrics.margin + static_cast<qreal>(rowIndex) * (m_metrics.keyHeight + m_metrics.gap);
        QList<PlacedKey> placedRow;
        for (const KeyDefinition &key : row) {
            const qreal width = unit * key.width;
            placedRow.append(PlacedKey{key.kind, labelFor(key), QRectF(x, y, width, m_metrics.keyHeight)});
            x += width + m_metrics.gap;
        }
        placed.append(placedRow);
    }
    return placed;
}

QVariantList OskKeyboardModel::rows() const
{
    QVariantList result;
    const auto placed = placedRows();
    for (const QList<PlacedKey> &row : placed) {
        QVariantList keys;
        for (const PlacedKey &key : row) {
            keys.append(QVariantMap{
                {QStringLiteral("kind"), keyKindName(key.kind)},
                {QStringLiteral("label"), key.label},
                {QStringLiteral("x"), key.rect.x()},
                {QStringLiteral("y"), key.rect.y()},
                {QStringLiteral("width"), key.rect.width()},
                {QStringLiteral("height"), key.rect.height()},
                {QStringLiteral("active"), (key.kind == KeyKind::Shift && m_shifted)},
            });
        }
        result.append(QVariant(keys));
    }
    return result;
}

std::optional<PlacedKey> OskKeyboardModel::keyAt(const QPointF &point) const
{
    const auto placed = placedRows();
    for (const QList<PlacedKey> &row : placed) {
        for (const PlacedKey &key : row) {
            if (key.rect.contains(point)) {
                return key;
            }
        }
    }
    return std::nullopt;
}

void OskKeyboardModel::showSymbols(bool symbols)
{
    const bool page = symbols && !m_document.symbols.isEmpty();
    if (page == m_symbolsPage && !m_shifted) {
        return;
    }
    m_symbolsPage = page;
    m_shifted = false;
    Q_EMIT rowsChanged();
}

void OskKeyboardModel::resetTransientState()
{
    if (!m_shifted && !m_symbolsPage) {
        return;
    }
    m_shifted = false;
    m_symbolsPage = false;
    Q_EMIT rowsChanged();
}

void OskKeyboardModel::pressText(const KeyDefinition &key)
{
    const QString text = m_shifted ? key.shiftedText : key.text;
    if (m_emitter != nullptr) {
        m_emitter->commitText(text);
    }
    Q_EMIT keyPressed(keyKindName(KeyKind::Text), text);
    if (m_shifted) {
        // Shift is a one-shot on a finger keyboard: the next letter is lower
        // case again, exactly as every phone keyboard behaves.
        m_shifted = false;
        Q_EMIT rowsChanged();
    }
}

void OskKeyboardModel::press(int row, int index)
{
    const QList<KeyRow> &page = currentPage();
    if (row < 0 || row >= page.size() || index < 0 || index >= page.at(row).size()) {
        return;
    }
    const KeyDefinition key = page.at(row).at(index);
    switch (key.kind) {
    case KeyKind::Text:
        pressText(key);
        return;
    case KeyKind::Space:
        if (m_emitter != nullptr) {
            m_emitter->commitText(QStringLiteral(" "));
        }
        break;
    case KeyKind::Enter:
        if (m_emitter != nullptr) {
            m_emitter->pressKeysym(XKB_KEY_Return);
        }
        break;
    case KeyKind::Backspace:
        if (m_emitter != nullptr) {
            m_emitter->pressKeysym(XKB_KEY_BackSpace);
        }
        break;
    case KeyKind::Shift:
        m_shifted = !m_shifted;
        Q_EMIT rowsChanged();
        break;
    case KeyKind::Symbols:
        showSymbols(true);
        break;
    case KeyKind::Letters:
        showSymbols(false);
        break;
    case KeyKind::Hide:
        Q_EMIT hideRequested();
        break;
    case KeyKind::Layout:
        Q_EMIT nextLayoutRequested();
        break;
    }
    Q_EMIT keyPressed(keyKindName(key.kind), QString());
}

} // namespace QindaQt::Apps::Osk
