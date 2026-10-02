# Complete Portage image discovery prepared

Use existing installed_artifact_path and run_installed_session from tests/session/test_installed_plugin_discovery.py against complete Portage image/usr. The CLI always calls stage_install and has no preinstalled option; direct existing-helper invocation avoids incomplete developer-build installation while preserving native/plugin/XWayland/workflow/decoration assertions. No --plugin-root override; compiled launcher bin/lib64/qt6/plugins defaults are relative and resolve image plugins.

Exact command template and helper/probe SHA bindings: ignored build/complete-portage-image-qa/command-plan.json. Helper byte-identical29f/manager4e021. Root supplies readable complete image only after Portage install phase success; private user namespace/runtime lease must be explicitly granted. No new source/harness/build/runtime/install. Next image gate, then actual/usr after root signedDesktop installation and fresh grant.
