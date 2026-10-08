# SPDX-License-Identifier: GPL-3.0-or-later
"""Injected typing sequence and witness negatives; no display/process action."""
import unittest,time
from pathlib import Path
from typing_probe import probe,admit_typing
class Fake:
    def __init__(self):self.calls=[];self.count=0
    def focus(self,w):self.calls.append("focus")
    def focus_within(self,w):return True
    def chord(self,w,c):self.calls.append(c)
    def key(self,w,c):self.calls.append(c)
    def flush(self):pass
    def capture(self,w,p):
        self.count+=1
        return {"sha256":str(self.count)*64,"pixelBytes":69120,"bytes":69134,"path":str(p)}
    def clipboard(self,d,g):g();return "qinda a 3108"
class TypingControls(unittest.TestCase):
    def value(self):
        return {"clipboardText":"qinda a 3108","focusWithinClient":True,
                "beforeSHA256":"a"*64,"afterSHA256":"b"*64,"pixelBytes":69120,
                "captures":[{"sha256":v*64,"pixelBytes":69120,"bytes":69134} for v in ["a","b"]]}
    def test_actual_sequence_preserves_scoped_witnesses(self):
        io=Fake();guards=[]
        v=probe(io,10,"qinda a 3108",lambda:guards.append(True),time.monotonic()+2,Path("/unused"))
        self.assertEqual(v["clipboardText"],"qinda a 3108")
        self.assertEqual(io.calls[0:3],["focus","a",0xff08])
        self.assertEqual(io.calls[-2:],["a","c"])
        self.assertGreater(len(guards),len("qinda a 3108"))
        self.assertFalse(v["physicalInput"]);self.assertFalse(v["renderedTextOCR"])
    def test_wrong_readback_refuses(self):
        v=self.value();v["clipboardText"]="other";self.assertRaises(RuntimeError,admit_typing,v,"qinda a 3108")
    def test_unchanged_pixels_refuse(self):
        v=self.value();v["afterSHA256"]=v["beforeSHA256"];self.assertRaises(RuntimeError,admit_typing,v,"qinda a 3108")
    def test_malformed_capture_witnesses_refuse(self):
        for field,bad in [("beforeSHA256",None),("beforeSHA256","z"*64),("afterSHA256","b"),
                          ("pixelBytes",1),("captures",[])]:
            v=self.value();v[field]=bad
            with self.subTest(field=field,bad=bad):self.assertRaises(RuntimeError,admit_typing,v,"qinda a 3108")
        for field in ["bytes","pixelBytes","sha256"]:
            v=self.value();v["captures"][0][field]=0
            self.assertRaises(RuntimeError,admit_typing,v,"qinda a 3108")
    def test_focus_loss_refuses(self):
        v=self.value();v["focusWithinClient"]=False;self.assertRaises(RuntimeError,admit_typing,v,"qinda a 3108")
    def test_incarnation_guard_failure_stops_before_input(self):
        io=Fake()
        def refuse():raise RuntimeError("replaced")
        with self.assertRaisesRegex(RuntimeError,"replaced"):
            probe(io,10,"qinda a 3108",refuse,time.monotonic()+2,Path("/unused"))
        self.assertEqual(io.calls,[])
    def test_deadline_stops_before_input(self):
        io=Fake()
        with self.assertRaisesRegex(RuntimeError,"deadline"):
            probe(io,10,"qinda a 3108",lambda:None,time.monotonic()-1,Path("/unused"))
        self.assertEqual(io.calls,[])
    def test_closed_alphabet(self):
        self.assertRaises(RuntimeError,probe,Fake(),10,"secret\n",lambda:None,time.monotonic()+2,Path("/unused"))
if __name__=="__main__":unittest.main()
