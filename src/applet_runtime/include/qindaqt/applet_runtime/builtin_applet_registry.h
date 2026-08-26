// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace QindaQt::AppletRuntime {

class BuiltinAppletRegistry final {
public:
    explicit BuiltinAppletRegistry(QStringList auditedEntryPoints = {});

    // AGENT-CONTRACT: this is the compiled-code trust root. A manifest
    // declaring `builtin` never adds itself here or makes code trusted.
    [[nodiscard]] static BuiltinAppletRegistry firstParty();
    [[nodiscard]] bool contains(QStringView entryPoint) const;
    [[nodiscard]] QStringList entryPoints() const;

private:
    QSet<QString> m_entryPoints;
};

} // namespace QindaQt::AppletRuntime
