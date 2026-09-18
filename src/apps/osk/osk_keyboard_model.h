// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "osk_layout_document.h"

#include <QList>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QVariantList>

namespace QindaQt::Apps::Osk {

// The seam between the keyboard and whatever delivers its output: the
// Wayland input-method context in production, a recorder in tests.
class KeyEmitter {
public:
    virtual ~KeyEmitter();
    virtual void commitText(const QString &text) = 0;
    virtual void pressKeysym(quint32 keysym) = 0;
};

// One placed key as QML paints it and as the evidence file records it.
struct PlacedKey {
    KeyKind kind = KeyKind::Text;
    QString label;
    QRectF rect;
};

// The keyboard's state machine and geometry: pages, shift, the placed key
// rectangles for one panel width, and what each press does. No Wayland, no
// QML types, so the rows it exposes are exactly what tests can assert.
class OskKeyboardModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged FINAL)
    Q_PROPERTY(bool shifted READ shifted NOTIFY rowsChanged FINAL)
    Q_PROPERTY(bool symbolsPage READ symbolsPage NOTIFY rowsChanged FINAL)
    Q_PROPERTY(QString layoutName READ layoutName NOTIFY rowsChanged FINAL)
    Q_PROPERTY(QString layoutLabel READ layoutLabel NOTIFY rowsChanged FINAL)
    Q_PROPERTY(qreal panelHeight READ panelHeight NOTIFY rowsChanged FINAL)

public:
    struct Metrics {
        qreal width = 0.0;
        qreal keyHeight = 52.0;
        qreal gap = 6.0;
        qreal margin = 10.0;
        qreal maximumUnit = 96.0;
    };

    explicit OskKeyboardModel(QObject *parent = nullptr);

    void setDocument(const OskLayoutDocument &document);
    [[nodiscard]] const OskLayoutDocument &document() const { return m_document; }
    void setEmitter(KeyEmitter *emitter) { m_emitter = emitter; }
    void setMetrics(const Metrics &metrics);
    [[nodiscard]] const Metrics &metrics() const { return m_metrics; }

    [[nodiscard]] QVariantList rows() const;
    [[nodiscard]] QList<QList<PlacedKey>> placedRows() const;
    [[nodiscard]] bool shifted() const { return m_shifted; }
    [[nodiscard]] bool symbolsPage() const { return m_symbolsPage; }
    [[nodiscard]] QString layoutName() const { return m_document.name; }
    [[nodiscard]] QString layoutLabel() const { return m_document.label; }
    [[nodiscard]] qreal panelHeight() const;
    [[nodiscard]] std::optional<PlacedKey> keyAt(const QPointF &point) const;

    Q_INVOKABLE void relayout(qreal width, qreal keyHeight, qreal gap, qreal margin);
    Q_INVOKABLE void press(int row, int index);
    void showSymbols(bool symbols);
    void resetTransientState();

Q_SIGNALS:
    void rowsChanged();
    void hideRequested();
    void nextLayoutRequested();
    void keyPressed(const QString &kind, const QString &text);

private:
    [[nodiscard]] const QList<KeyRow> &currentPage() const;
    [[nodiscard]] QString labelFor(const KeyDefinition &key) const;
    void pressText(const KeyDefinition &key);

    OskLayoutDocument m_document;
    KeyEmitter *m_emitter = nullptr;
    Metrics m_metrics;
    bool m_shifted = false;
    bool m_symbolsPage = false;
};

} // namespace QindaQt::Apps::Osk
