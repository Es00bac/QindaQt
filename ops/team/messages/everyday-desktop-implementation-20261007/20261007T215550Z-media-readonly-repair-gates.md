# Exact readonly repair gates and resource release

- Worker: everyday_media_delivery
- Source tested: 823738ab1b788d1b657886c7a9b16a370580e7d9 in a detached readonly verification worktree, clean tracked tree.
- Configure: standalone media entry point, Debug, ccache, strict Wall/Wextra/Wpedantic/Werror; exit 0.
- Build: cmake --build .cache/media-inventory-build -- -j24 -l24; exit 0.
- CTest: 4/4, zero failures. Direct Qt totals: 10 policy, 10 UDisks, 6 notifications, 11 inventory; 37 passed, zero failed/skipped.
- New inventory cases: transient/deadline discovery retry; recovery during initial lookup retains Loading; single-flight reads/replacement late reply and forged old signal fencing.
- Installed SDK: separate installed_consumer uses staged headers/archives only, build exit 0 and 1/1. Withheld staged media_client.h causes expected missing-header compile failure. Header restored by bounded trap, restored build exit 0 and 1/1.
- Logs: ignored .cache/media-inventory-* under the detached readonly verification tree.
- Resource: compiler/private-session-bus lease explicitly RELEASED to Astra after gates. Absent system bus; no host helper, mount, device or physical USB action.
- Scope: readonly repair only; unfinished ordinary actions/File Manager/chooser sources remain in the separate delivery tree and gain no qualification from this gate.
- Requested next: independent exact readonly acceptance/integration; queue the complete ordinary consumer source packet for its own compiler/test/review gate.
