// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QLoggingCategory>
// AGENT-CONTRACT: every way the locker can end, retry or give up is logged here
// with its reason (stderr reaches the compositor's journal). Messages carry
// state names and counts only: never prompts, responses, account names beyond
// what the journal already holds, or other credential material.
Q_DECLARE_LOGGING_CATEGORY(lcGreeter)
