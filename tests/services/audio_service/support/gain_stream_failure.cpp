// SPDX-License-Identifier: GPL-3.0-or-later
// Test-executable-only ELF interposition of the public allocator. The actual
// original/patched module still executes init/listener/cleanup code unchanged.
// The counter proves the fault was after hook registration in setup_streams,
// not a malformed-argument rejection before the Core listeners exist.
#include <pipewire/pipewire.h>
#include <dlfcn.h>
#include <cerrno>
#include <cstdlib>
namespace {
thread_local bool failNext = false;
thread_local unsigned failures = 0;
}
extern "C" void qindaqt_gain_fail_next_stream_new() { failNext = true; }
extern "C" unsigned qindaqt_gain_stream_failures() { return failures; }

extern "C" __attribute__((visibility("default")))
pw_stream *pw_stream_new(pw_core *core, const char *name, pw_properties *properties)
{
    if (failNext) {
        failNext = false;
        ++failures;
        // pw_stream_new takes the properties even on allocation failure.
        pw_properties_free(properties);
        errno = ENOMEM;
        return nullptr;
    }
    using Function = pw_stream *(*)(pw_core *, const char *, pw_properties *);
    static auto next = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "pw_stream_new"));
    if (!next) std::abort();
    return next(core, name, properties);
}
