import unittest
from vm_plan import argv, admit_envelope, minimal_environment, AFFINITY, manifest_profile

class PlanTests(unittest.TestCase):
    def values(self):
        return {"memory.max": str(12 * 1024**3), "memory.swap.max": "0",
                "pids.max": "256", "cpu.max": "800000 100000", "nice": 10}
    def test_exact_bounded_envelope(self):
        admit_envelope(self.values(), AFFINITY)
    def test_unbounded_or_excess_resources_refused(self):
        for key, value in (("memory.max", "max"), ("memory.swap.max", "1"),
                           ("pids.max", "257"), ("cpu.max", "900000 100000"),
                           ("cpu.max", "max 100000"), ("nice", 0)):
            with self.subTest(key=key, value=value):
                values = self.values(); values[key] = value
                with self.assertRaises(ValueError):
                    admit_envelope(values, AFFINITY)
    def test_wrong_affinity_refused(self):
        with self.assertRaises(ValueError):
            admit_envelope(self.values(), {23})
    def test_fixed_no_network_or_host_export(self):
        command = argv(dict(kernel=3, initramfs=4, system=5, vendor=6))
        self.assertEqual(command[command.index("-nic") + 1], "none")
        self.assertNotIn("-virtfs", command)
        self.assertNotIn("-fsdev", command)
        self.assertEqual(sum("readonly=on" in x for x in command), 2)
        self.assertNotIn("DISPLAY", minimal_environment())
    def test_input_alias_and_extra_refused(self):
        for fds in (dict(kernel=3, initramfs=4, system=5, vendor=5),
                    dict(kernel=3, initramfs=4, system=5, vendor=6, extra=7)):
            with self.assertRaises(ValueError): argv(fds)

    def test_qinda_default_argv_is_unchanged(self):
        fds = dict(kernel=3, initramfs=4, system=5, vendor=6)
        self.assertEqual(argv(fds), argv(fds, "qinda"))
        self.assertEqual(manifest_profile({"schema": 1}), "qinda")
        command = argv(fds)
        self.assertEqual(command[command.index("-m")+1], "8192")
        self.assertEqual(command[command.index("-smp")+1], "8")
    def test_laptop_fixed_guest_and_shared_isolation(self):
        fds = dict(kernel=3, initramfs=4, system=5, vendor=6)
        laptop, qinda = argv(fds, "laptop"), argv(fds)
        for option, expected in (("-m", "4096"), ("-smp", "4")):
            index = laptop.index(option)+1
            self.assertEqual(laptop[index], expected)
            laptop[index] = qinda[index]
        self.assertEqual(laptop, qinda)
        self.assertEqual(manifest_profile({"resourceProfile": "laptop"}), "laptop")
    def test_laptop_live_caps(self):
        values = self.values()
        values.update({"memory.max": str(5*1024**3), "cpu.max": "400000 100000"})
        admit_envelope(values, set(range(8)), "laptop")
        for key, value in (("memory.max", str(5*1024**3+1)),
                           ("cpu.max", "400001 100000"),
                           ("memory.swap.max", "1"), ("pids.max", "257"),
                           ("nice", 9)):
            altered = dict(values); altered[key] = value
            with self.subTest(key=key):
                with self.assertRaises(ValueError):
                    admit_envelope(altered, set(range(8)), "laptop")
        with self.assertRaises(ValueError):
            admit_envelope(values, {0, 8}, "laptop")
    def test_laptop_cannot_admit_qinda_envelope(self):
        with self.assertRaises(ValueError):
            admit_envelope(self.values(), set(range(8)), "laptop")
    def test_unknown_or_nonstring_profile_refused(self):
        for name in ("auto", "", None, {}, 4):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    manifest_profile({"resourceProfile": name})
                with self.assertRaises(ValueError):
                    argv(dict(kernel=3, initramfs=4, system=5, vendor=6), name)

if __name__ == "__main__": unittest.main()
