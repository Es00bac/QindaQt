# SPDX-License-Identifier: GPL-3.0-or-later
import json, pathlib, subprocess, sys, tempfile, unittest
PROGRAM=sys.argv.pop()
class PublisherTests(unittest.TestCase):
    def test_projection(self):
        with tempfile.TemporaryDirectory() as d:
            raw={"transcript_path":"/private/never-read","secret":"must-not-persist","context_window":{"total_input_tokens":9},
                 "cost":{"total_cost_usd":0.2},"rate_limits":{"five_hour":{"used_percentage":10,"resets_at":2000000000}}}
            r=subprocess.run([PROGRAM,"--provider","claude","--claude-statusline","--directory",d],input=json.dumps(raw).encode(),capture_output=True)
            self.assertEqual(r.returncode,0);s=(pathlib.Path(d)/"claude.json").read_text();report=json.loads(s)
            self.assertNotIn("private",s);self.assertNotIn("secret",s);self.assertEqual(report["tokenScope"],"context")
            self.assertEqual(report["costScope"],"session");self.assertNotIn("totalTokens",report)
            self.assertEqual((pathlib.Path(d)/"claude.json").stat().st_mode & 0o777,0o600)
    def test_stdin_expiry(self):
        with tempfile.TemporaryDirectory() as d:
            p=subprocess.Popen([PROGRAM,"--provider","claude","--claude-statusline","--directory",d],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            try:
                self.assertEqual(p.wait(timeout=7),1)
                self.assertEqual(p.stderr.read(),b"Agent usage report rejected\n")
                self.assertEqual(list(pathlib.Path(d).iterdir()),[])
            finally:
                p.stdin.close()
                p.stdout.close()
                p.stderr.close()
                if p.poll() is None:
                    p.kill()
                    p.wait()
    def test_rejection(self):
        for raw in (b'{"x":1,"x":2}',b"x"*65537,b'{"context_window":{"total_input_tokens":-1}}'):
            with tempfile.TemporaryDirectory() as d:
                r=subprocess.run([PROGRAM,"--provider","claude","--claude-statusline","--directory",d],input=raw,capture_output=True)
                self.assertEqual(r.returncode,1);self.assertEqual(list(pathlib.Path(d).iterdir()),[])
                self.assertEqual(r.stderr,b"Agent usage report rejected\n")
unittest.main()
