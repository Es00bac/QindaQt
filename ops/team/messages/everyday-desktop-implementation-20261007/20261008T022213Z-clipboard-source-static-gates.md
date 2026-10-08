# Exact clipboard source static verification

Candidate06463d7b370b8b4c57b9a2a21cd972615c91208a, basea6f0953d5acd87cdb9451c03aa596d629b9fa9a9. This descendant changes only own worker record/receipt; production/tests/docs remain the frozen candidate.

Actual commands: python3 tools/validate-docs --root $PWD (exit0,531 Markdown documents), mkdocs build --strict --site-dir .cache/clipboard-static/site (exit0), python3 tools/check-source-shape --root src/services/clipboard_service --config tools/source-shape.json --warnings-as-errors --json (exit0,14files/0issues), same root tests/services/clipboard_service (exit0,15files/0issues), git diff --check (exit0), cmake -DSOURCE_ROOT=$PWD -P tests/services/clipboard_service/check_boundary.cmake (exit0).

Production largest nonblank lines: Host173, private privacy99, main97, native observer96. Test largest: admission253, native startup206, native helper110. No required size decomposition waiver.

Ignored exact evidence JSON/logs: .cache/clipboard-static/evidence.json, docs.log, mkdocs.log, production-shape.json, tests-shape.json. All hash values independently calculated from actual bytes in this worker.

No compiler, CTest, old/fixed native, SDK symbol/header poison or installed fresh-login/capture execution. Root alone owns current native lane. Source remains unqualified until exact trust review and assigned native gates. Initial owner/socket ordering must be exercised under ordinary installed fresh login before the repair is claimed. No host clipboard content probe/capture or install action.
