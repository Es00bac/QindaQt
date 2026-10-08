// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/app_shell/action_registry.h>

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Viewer {
class ViewerController;
QList<AppShell::ActionSpec> viewerActions();
void updateViewerActions(AppShell::ApplicationCoordinator &coordinator,
                         const ViewerController &viewer);
}
