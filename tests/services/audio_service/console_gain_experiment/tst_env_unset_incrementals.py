# SPDX-License-Identifier: GPL-3.0-or-later
"""Pure installed Portage config reads; no phases, subprocesses or package action."""
import unittest,tempfile
from pathlib import Path
import portage
from cohort_qualification import preserved_env_unset
from portage_environment import admit_effective_environment,admit_saved_environment

class IncrementalEnvironment(unittest.TestCase):
    def test_actual_portage_expansion_preserves_private_bus(self):
        baseline=portage.config().get("ENV_UNSET","")
        env={"ENV_UNSET":preserved_env_unset(baseline),
             "DBUS_SESSION_BUS_ADDRESS":"unix:path=/nonexistent/private-session",
             "DBUS_SYSTEM_BUS_ADDRESS":"unix:path=/nonexistent/private-system"}
        observed=admit_effective_environment(env,portage.config)
        for key in env:
            if key!="ENV_UNSET":self.assertEqual(observed[key],env[key])
        expected=set(baseline.split())-{"DBUS_SESSION_BUS_ADDRESS","DBUS_SYSTEM_BUS_ADDRESS"}
        self.assertEqual(set(observed["ENV_UNSET"].split()),expected)

    def test_profile_positive_reintroduction_refuses(self):
        env={"ENV_UNSET":"-DBUS_SESSION_BUS_ADDRESS -DBUS_SYSTEM_BUS_ADDRESS",
             "DBUS_SESSION_BUS_ADDRESS":"unix:path=/nonexistent/session",
             "DBUS_SYSTEM_BUS_ADDRESS":"unix:path=/nonexistent/system"}
        with self.assertRaisesRegex(ValueError,"filters private bus"):
            admit_effective_environment(env,lambda **unused:{**env,"ENV_UNSET":"DISPLAY DBUS_SESSION_BUS_ADDRESS"})

    def test_changed_effective_address_refuses(self):
        env={"ENV_UNSET":"-DBUS_SESSION_BUS_ADDRESS -DBUS_SYSTEM_BUS_ADDRESS",
             "DBUS_SESSION_BUS_ADDRESS":"unix:path=/nonexistent/session",
             "DBUS_SYSTEM_BUS_ADDRESS":"unix:path=/nonexistent/system"}
        with self.assertRaisesRegex(ValueError,"address changed"):
            admit_effective_environment(env,lambda **unused:{**env,"ENV_UNSET":"DISPLAY",
                "DBUS_SESSION_BUS_ADDRESS":"unix:path=/unexpected"})

    def test_saved_environment_values_and_refusals(self):
        env={"ENV_UNSET":"-DBUS_SESSION_BUS_ADDRESS -DBUS_SYSTEM_BUS_ADDRESS",
             "DBUS_SESSION_BUS_ADDRESS":"unix:path=/nonexistent/session",
             "DBUS_SYSTEM_BUS_ADDRESS":"unix:path=/nonexistent/system"}
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/"environment"
            good='declare -x ENV_UNSET="DISPLAY"\n'+''.join(
                'declare -x '+key+'="'+env[key]+'"\n' for key in env if key!="ENV_UNSET")
            path.write_text(good)
            self.assertEqual(admit_saved_environment(path,env)["ENV_UNSET"],"DISPLAY")
            for bad in [good.replace('ENV_UNSET="DISPLAY"','ENV_UNSET="DISPLAY DBUS_SESSION_BUS_ADDRESS"'),
                        good.replace(env["DBUS_SESSION_BUS_ADDRESS"],"unix:path=/host"),
                        good.splitlines(True)[0]+good.splitlines(True)[2],
                        good+good.splitlines(True)[1]]:
                path.write_text(bad)
                with self.assertRaises(ValueError):admit_saved_environment(path,env)

if __name__=="__main__":unittest.main()
