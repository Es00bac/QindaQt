// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <memory>

class QGuiApplication;

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: the File Manager process application object, composed once
// by main() and by focused tests through this single factory so a test
// exercises KIO prompt paths under the same application class as production.
// KIO's standard job UI delegate (KIOWidgets, ADR-0151/0152) prompts with
// QWidgets -- credential/message boxes and the Open With dialog -- which
// abort a bare QGuiApplication; the returned object is therefore
// QWidget-capable even though the File Manager UI itself is Qt Quick
// (ADR-0116). The caller owns the result; argc/argv must outlive it, and no
// QCoreApplication may exist when this is called.
[[nodiscard]] std::unique_ptr<QGuiApplication> createApplication(int &argc, char **argv);

} // namespace QindaQt::Apps::FileManager
