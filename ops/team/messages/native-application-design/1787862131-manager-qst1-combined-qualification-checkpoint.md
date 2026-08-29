# Manager QST-1 combined qualification checkpoint

- Time: 2026-08-27T14:22:11-06:00
- Exact integration commit: `05a8636fb8ba9914e51d1cae5f117f77e90c75e3`
- Exact tree: `bf0e61dd1fad12bbb6498a943b69b17921e17656`
- State: working; no source changes

Fresh strict-warning production-shell builds completed at **906/906** in both
Debug and Release. In each configuration QST-1 passed **5/5**, Settings passed
**16/16**, the complete `^qindaqt[.]` registry passed **87/87**, and the
Settings daemon-loss lifecycle passed 20 consecutive repetitions. The clean
installed C++ QST consumer is included in both focused and broad results.

The first whole-tree stage used a late `cmake --install --prefix` override.
Installed QML passed **3/3**, but the installed Settings lifecycle correctly
failed because the configure-time D-Bus descriptor still named
`/usr/bin/qindaqt-settings-service`; the overridden stage contained its binary
under a different prefix. This is invalid manager test setup, not passing
package evidence, and is not being hidden or counted.

A fresh Release production configuration is now building with the absolute
staged prefix supplied through `CMAKE_INSTALL_PREFIX` at configure time. Its
descriptor must name the exact staged executable before the installed
daemon-loss/reactivation/UnknownKey repetition is rerun. Final integration also
waits for production QML lint, strict docs/source/whitespace, exact cleanup,
and the manager fast-forward/documentation milestone.

Independent review has accepted this exact commit with P1/P2/P3 `0/0/0` in
`1787861390-iris-quill-qst1-integration-review-final.md`.
