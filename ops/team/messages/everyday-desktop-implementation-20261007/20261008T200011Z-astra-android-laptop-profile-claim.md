# Android laptop fixture profile claim
- Time: 2026-10-08T20:00:11.805016+00:00
- Exact base: f34306594e3239888b45380de659bdf8664e2e1d
- Outcome: reuse the existing stock fixture with4vCPU/4096MiB guest,5GiB host cap,4CPU quota/0-7 affinity, jobs/load4 andnice10, preserving qinda profile.
- Owned paths: tools/foreign-runtime/android-stock-proof/vm_plan.py and test_vm_plan.py; minimal run_vm.py profile threading; README.md/RUNTIME-INPUTS.md and laptop input manifest; own records.
- Source and read-only installed VDB/payload input preparation only. No compiler, Portage, stage boot, guest init, host Waydroid or VM execution.
- Actual laptop topology confirms0-7 are four physical cores with both SMT threads (pairs0-1,2-3,4-5,6-7); four other physical cores remain outside affinity.
- Inventory must bind Portage-owned selected public payload bytes and full runtime dependencies without copying /etc,/home,/run or secrets. Full closure/boot is unqualified until actual validation.
