# SPDX-License-Identifier: GPL-3.0-or-later
"""Injected kernel-wait controls; zero/live and expiry never qualify retirement."""
import unittest
from types import SimpleNamespace
from domain_ledger import DomainLedger,kernel_empty
def empty(unused,flags):raise ChildProcessError()
def ledger(clock,waiter):
    result=object.__new__(DomainLedger);result.clock=clock;result.waiter=waiter
    result.failed=False;result.reaped=[]
    result.direct={k:SimpleNamespace(reaped=True) for k in ["server","app"]}
    return result
class Retirement(unittest.TestCase):
    def test_kernel_empty_not_proc(self):
        self.assertTrue(kernel_empty(empty))
        self.assertFalse(kernel_empty(lambda p,f:(0,0)))
        with self.assertRaises(RuntimeError):kernel_empty(lambda p,f:(42,0))
    def test_past_deadline(self):
        with self.assertRaises(RuntimeError):ledger(lambda:101,empty).final(100)
    def test_crossing_echild(self):
        ticks=iter([99,101])
        with self.assertRaises(RuntimeError):ledger(lambda:next(ticks),empty).final(100)
    def test_zero_remains_live_until_expiry(self):
        ticks=iter([99,99,101]);calls=[]
        l=ledger(lambda:next(ticks),lambda p,f:(calls.append("live") or 0,0))
        with self.assertRaises(RuntimeError):l.final(100)
        self.assertEqual(calls,["live"]);self.assertTrue(l.failed)
    def test_reap_then_echild(self):
        values=iter([(42,0)])
        def waiter(p,f):
            try:return next(values)
            except StopIteration:raise ChildProcessError()
        value=ledger(lambda:99,waiter).final(100)
        self.assertTrue(value["kernelECHILD"]);self.assertEqual(value["adoptedSignals"],0)
        self.assertEqual(value["adoptedReaped"],[{"pid":42,"status":0}])
    def test_nonzero_or_signaled_refused(self):
        for status in [256,9]:
            with self.assertRaises(RuntimeError):ledger(lambda:99,lambda p,f:(42,status)).final(100)
    def test_direct_waiter_order(self):
        l=ledger(lambda:99,empty);l.direct["app"].reaped=False
        with self.assertRaises(RuntimeError):l.final(100)
if __name__=="__main__":unittest.main()
