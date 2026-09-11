// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_layouts_model.h>

namespace QindaQt::Apps::SettingsInput {

LayoutCatalogModel::LayoutCatalogModel(QObject *parent)
    : QAbstractListModel(parent) {}

bool LayoutCatalogModel::load(const QList<EvdevLayoutOption> &layouts) {
    beginResetModel();
    m_layouts = layouts;
    endResetModel();
    Q_EMIT countChanged();
    return !m_layouts.isEmpty();
}

int LayoutCatalogModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_layouts.size());
}

QVariant LayoutCatalogModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= int(m_layouts.size())) {
        return {};
    }
    const EvdevLayoutOption &option = m_layouts.at(index.row());
    switch (role) {
    case CodeRole:
        return option.code;
    case DescriptionRole:
        return option.description;
    case VariantsRole: {
        QVariantList variants;
        variants.reserve(option.variants.size());
        for (const EvdevVariantOption &variant : option.variants) {
            variants.append(QVariantMap{
                {QStringLiteral("code"), variant.code},
                {QStringLiteral("description"), variant.description},
            });
        }
        return variants;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> LayoutCatalogModel::roleNames() const {
    return {
        {CodeRole, "code"},
        {DescriptionRole, "description"},
        {VariantsRole, "variants"},
    };
}

QString LayoutCatalogModel::descriptionFor(const QString &code) const {
    for (const EvdevLayoutOption &option : m_layouts) {
        if (option.code == code) {
            return option.description;
        }
    }
    return code;
}

QVariantMap LayoutCatalogModel::layoutAt(int row) const {
    if (row < 0 || row >= m_layouts.size()) {
        return {};
    }
    const EvdevLayoutOption &option = m_layouts.at(row);
    QVariantList variants;
    for (const EvdevVariantOption &variant : option.variants) {
        variants.append(QVariantMap{
            {QStringLiteral("code"), variant.code},
            {QStringLiteral("description"), variant.description},
        });
    }
    return QVariantMap{
        {QStringLiteral("code"), option.code},
        {QStringLiteral("description"), option.description},
        {QStringLiteral("variants"), variants},
    };
}

QString LayoutCatalogModel::variantDescriptionFor(const QString &layout,
                                                  const QString &variant) const {
    for (const EvdevLayoutOption &option : m_layouts) {
        if (option.code != layout) {
            continue;
        }
        for (const EvdevVariantOption &candidate : option.variants) {
            if (candidate.code == variant) {
                return candidate.description;
            }
        }
    }
    return variant;
}

KeyboardLayoutsModel::KeyboardLayoutsModel(const KeyboardLayoutPort &port,
                                           QObject *parent)
    : QAbstractListModel(parent), m_port(port),
      m_catalog(new LayoutCatalogModel(this)) {}

int KeyboardLayoutsModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : int(m_layouts.size());
}

QVariant KeyboardLayoutsModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() >= int(m_layouts.size())) {
        return {};
    }
    const KeyboardLayoutSelection &row = m_layouts.at(index.row());
    switch (role) {
    case LayoutRole:
        return row.layout;
    case VariantRole:
        return row.variant;
    case TitleRole:
        return m_catalog->descriptionFor(row.layout);
    case VariantTitleRole:
        return m_catalog->variantDescriptionFor(row.layout, row.variant);
    default:
        return {};
    }
}

QHash<int, QByteArray> KeyboardLayoutsModel::roleNames() const {
    return {
        {LayoutRole, "layout"},
        {VariantRole, "variant"},
        {TitleRole, "title"},
        {VariantTitleRole, "variantTitle"},
    };
}

void KeyboardLayoutsModel::setCatalogPath(const QString &path) {
    m_catalogPath = path;
}

void KeyboardLayoutsModel::ensureCatalogLoaded() {
    if (m_catalogLoaded || m_catalogPath.isEmpty()) {
        return;
    }
    m_catalogLoaded = true;
    QString error;
    const QList<EvdevLayoutOption> layouts =
        EvdevLayoutCatalog::parse(m_catalogPath, &error);
    if (!m_catalog->load(layouts)) {
        // AGENT-GUARD: An unparsable catalog leaves the picker empty and
        // the add-layout flow degraded; it must never leave a half-loaded
        // catalog that could bless unknown layout codes into kxkbrc.
        m_statusText = error;
        Q_EMIT statusTextChanged();
    }
}

void KeyboardLayoutsModel::refresh() {
    ensureCatalogLoaded();
    QString error;
    const QList<KeyboardLayoutSelection> layouts =
        m_port.configuredLayouts(&error);
    const bool authorityAnswered = error.isEmpty();
    beginResetModel();
    m_layouts = layouts;
    m_available = authorityAnswered;
    endResetModel();
    Q_EMIT availableChanged();
    Q_EMIT countChanged();
}

bool KeyboardLayoutsModel::addLayout(const QString &layout,
                                     const QString &variant) {
    if (int(m_layouts.size()) >= 32 || layout.isEmpty()) {
        return false;
    }
    for (const KeyboardLayoutSelection &row : m_layouts) {
        if (row.layout == layout) {
            m_statusText = tr("Layout %1 is already in the list").arg(layout);
            Q_EMIT statusTextChanged();
            return false;
        }
    }
    // AGENT-GUARD: Only catalog-known codes may enter the list. The list is
    // fed straight into kxkbrc, and the catalog is the only source that has
    // validated the identifier shape and existence.
    bool known = false;
    for (const EvdevLayoutOption &option : m_catalog->layouts()) {
        if (option.code == layout) {
            known = true;
            break;
        }
    }
    if (!known) {
        m_statusText = tr("Unknown layout %1").arg(layout);
        Q_EMIT statusTextChanged();
        return false;
    }
    KeyboardLayoutSelection row;
    row.layout = layout;
    row.variant = variant;
    if (!row.isValidIdentifier()) {
        return false;
    }
    const int position = int(m_layouts.size());
    beginInsertRows(QModelIndex(), position, position);
    m_layouts.append(row);
    endInsertRows();
    Q_EMIT countChanged();
    return true;
}

void KeyboardLayoutsModel::removeRow(int row) {
    if (row < 0 || row >= int(m_layouts.size()) || m_layouts.size() <= 1) {
        // AGENT-GUARD: The last layout cannot be removed; the port refuses
        // an empty list, so the UI must never offer to produce one.
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_layouts.removeAt(row);
    endRemoveRows();
    Q_EMIT countChanged();
}

void KeyboardLayoutsModel::moveUp(int row) {
    if (row <= 0 || row >= int(m_layouts.size())) {
        return;
    }
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
    m_layouts.swapItemsAt(row, row - 1);
    endMoveRows();
}

void KeyboardLayoutsModel::moveDown(int row) {
    if (row < 0 || row >= int(m_layouts.size()) - 1) {
        return;
    }
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
    m_layouts.swapItemsAt(row, row + 1);
    endMoveRows();
}

void KeyboardLayoutsModel::apply() {
    if (m_applying) {
        return;
    }
    m_applying = true;
    Q_EMIT applyingChanged();
    QString error;
    const StoreResult result = m_port.writeConfiguredLayouts(m_layouts, &error);
    m_applying = false;
    Q_EMIT applyingChanged();
    switch (result) {
    case StoreResult::Stored:
        m_statusText = tr("Keyboard layouts applied");
        break;
    case StoreResult::StoredButReloadFailed:
        m_statusText = tr(
            "Saved. The running desktop could not be told to reload, so it "
            "applies on the next session.");
        break;
    case StoreResult::Failed:
        m_statusText = error.isEmpty()
                           ? tr("Keyboard layouts could not be stored")
                           : error;
        break;
    }
    Q_EMIT statusTextChanged();
}

} // namespace QindaQt::Apps::SettingsInput
