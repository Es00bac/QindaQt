// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_classification.h>

#include <QVariant>

namespace QindaQt::Services::TabletDevices {

TabletClassification classifyTablet(const TabletDeviceSnapshot &tool,
                                    const QList<TabletOutputCandidate> &outputs) {
    // AGENT-GUARD: only a tool's flag is libinput's verdict. KWin publishes
    // `supportsInputArea` on every input device, and a pad or a plain pointer
    // reports false because it has no tablet area at all, not because it is
    // a screen.
    const QVariant indirect =
        tool.properties.value(QStringLiteral("supportsInputArea"));
    if (tool.tabletTool && indirect.typeId() == QMetaType::Bool) {
        return TabletClassification{
            indirect.toBool() ? TabletKind::DeskTablet : TabletKind::PenDisplay,
            TabletKindEvidence::LibinputDirectness};
    }
    if (matchTabletOutput(tool, outputs).decided()) {
        return TabletClassification{TabletKind::PenDisplay,
                                    TabletKindEvidence::OwnScreen};
    }
    return TabletClassification{TabletKind::DeskTablet,
                                TabletKindEvidence::Default};
}

} // namespace QindaQt::Services::TabletDevices
