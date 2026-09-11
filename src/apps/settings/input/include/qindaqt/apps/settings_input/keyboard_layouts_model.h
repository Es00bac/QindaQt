// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAbstractListModel>

#include <qindaqt/apps/settings_input/evdev_layout_catalog.h>
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>

namespace QindaQt::Apps::SettingsInput {

// The installed xkb layout catalog as picker rows: one row per layout, with
// its variants embedded for the variant picker. Built once from the parsed
// evdev catalog; never written to.
class LayoutCatalogModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        CodeRole = Qt::UserRole + 1,
        DescriptionRole,
        // QVariantList of {code, description} maps for the variant picker.
        VariantsRole,
    };

    explicit LayoutCatalogModel(QObject *parent = nullptr);

    // Returns false (and leaves the model empty) when parsing fails, so the
    // route can degrade to the unavailable notice.
    bool load(const QList<EvdevLayoutOption> &layouts);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return int(m_layouts.size()); }
    [[nodiscard]] const QList<EvdevLayoutOption> &layouts() const {
        return m_layouts;
    }
    // Q_INVOKABLE: display truth for an already-configured row.
    Q_INVOKABLE QString descriptionFor(const QString &code) const;
    // Q_INVOKABLE: one picker row as {code, description, variants}; an
    // out-of-range row returns an empty map.
    Q_INVOKABLE QVariantMap layoutAt(int row) const;
    [[nodiscard]] QString variantDescriptionFor(const QString &layout,
                                                const QString &variant) const;

Q_SIGNALS:
    void countChanged();

private:
    QList<EvdevLayoutOption> m_layouts;
};

// The configured layout list (kxkbrc [Layout]) with add/remove/reorder and
// an explicit apply step. Titles are resolved through the catalog model
// when present; unknown codes fall back to their raw identifier.
class KeyboardLayoutsModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY applyingChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QObject *catalog READ catalog CONSTANT)

public:
    enum Roles {
        LayoutRole = Qt::UserRole + 1,
        VariantRole,
        TitleRole,
        VariantTitleRole,
    };

    explicit KeyboardLayoutsModel(const KeyboardLayoutPort &port,
                                  QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return int(m_layouts.size()); }
    [[nodiscard]] bool available() const { return m_available; }
    [[nodiscard]] bool applying() const { return m_applying; }
    [[nodiscard]] QString statusText() const { return m_statusText; }
    [[nodiscard]] LayoutCatalogModel *catalog() const { return m_catalog; }

    // Called by the composition root; no file IO happens here. Parsing the
    // catalog is deferred to the first refresh() so importing the module
    // stays cheap for every route.
    void setCatalogPath(const QString &path);
    // Q_INVOKABLE: reads kxkbrc and, on the first call, the evdev catalog;
    // called when the tab becomes visible.
    Q_INVOKABLE void refresh();
    // Q_INVOKABLE: appends layout (with optional variant) if valid, not a
    // duplicate, and the catalog knows the code (when a catalog is loaded).
    Q_INVOKABLE bool addLayout(const QString &layout,
                               const QString &variant);
    Q_INVOKABLE void removeRow(int row);
    Q_INVOKABLE void moveUp(int row);
    Q_INVOKABLE void moveDown(int row);
    // Q_INVOKABLE: persists the presented list through the port.
    Q_INVOKABLE void apply();

Q_SIGNALS:
    void countChanged();
    void availableChanged();
    void applyingChanged();
    void statusTextChanged();

private:
    void ensureCatalogLoaded();
    const KeyboardLayoutPort &m_port;
    LayoutCatalogModel *m_catalog;
    QList<KeyboardLayoutSelection> m_layouts;
    QString m_catalogPath;
    bool m_catalogLoaded = false;
    bool m_available = true;
    bool m_applying = false;
    QString m_statusText;
};

} // namespace QindaQt::Apps::SettingsInput
