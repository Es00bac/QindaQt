import unittest
from vm_plan import argv, admit_envelope, minimal_environment, AFFINITY

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

if __name__ == "__main__": unittest.main()
