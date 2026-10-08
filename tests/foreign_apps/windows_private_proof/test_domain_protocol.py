# SPDX-License-Identifier: GPL-3.0-or-later
"""Pure generation/deadline/schema controls; no process or installed program."""
import unittest
from domain_protocol import request,receipt,require_time,encode
class Protocol(unittest.TestCase):
    def setUp(self):
        self.nonce="a"*64;self.owner={"pid":10,"parent":1,"starttime":20}
        self.call={"domain":"app-a","nonce":self.nonce,"sequence":1,"operation":"checkpoint"}
        self.reply={"domain":"app-a","nonce":self.nonce,"sequence":1,
                    "supervisor":self.owner,"passed":True,"payload":{}}
    def test_exact(self):
        self.assertEqual(request(self.call,"app-a",self.nonce,1),"checkpoint")
        self.assertEqual(receipt(self.reply,"app-a",self.nonce,1,self.owner),{})
    def test_nonce_sequence_domain(self):
        for name,value in [("nonce","b"*64),("sequence",0),("domain","app-b")]:
            with self.subTest(name=name):
                c=dict(self.call);c[name]=value
                with self.assertRaises(RuntimeError):request(c,"app-a",self.nonce,1)
                r=dict(self.reply);r[name]=value
                with self.assertRaises(RuntimeError):receipt(r,"app-a",self.nonce,1,self.owner)
    def test_replaced_supervisor(self):
        for field in ["pid","starttime","parent"]:
            r=dict(self.reply);r["supervisor"]={**self.owner,field:999}
            with self.assertRaises(RuntimeError):receipt(r,"app-a",self.nonce,1,self.owner)
    def test_failed_and_malformed(self):
        for r in [{**self.reply,"passed":False},{**self.reply,"payload":[]},
                  {**self.reply,"extra":True},{**self.reply,"sequence":True}]:
            with self.assertRaises(RuntimeError):receipt(r,"app-a",self.nonce,1,self.owner)
    def test_operation_closed(self):
        for op in ["signal","launch-other","retire-peer"]:
            with self.assertRaises(RuntimeError):request({**self.call,"operation":op},"app-a",self.nonce,1)
    def test_expired_and_output_bound(self):
        with self.assertRaises(RuntimeError):require_time(100,lambda:101)
        with self.assertRaises(RuntimeError):encode({"x":"x"*65536})
if __name__=="__main__":unittest.main()
