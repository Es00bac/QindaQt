# SPDX-License-Identifier: GPL-3.0-or-later
"""Injected parent-edge controls; no process creation or signal."""
import unittest
from unittest.mock import patch
from domain_ledger import DomainLedger

class ParentEdges(unittest.TestCase):
    def test_stale_child_list_foreign_replacement(self):
        parent={"pid":100,"parent":1,"starttime":10,"state":"S"}
        foreign={"pid":200,"parent":999,"starttime":99,"state":"S"}
        with patch("domain_ledger.census_identity",return_value=parent):
            with self.assertRaisesRegex(RuntimeError,"causal parent edge"):
                DomainLedger.check_edge(parent,foreign)

    def test_checkpoint_stale_child_list_foreign_replacement(self):
        # Exercise the actual admission path, not only its pure edge predicate.
        parent={"pid":100,"parent":1,"starttime":10,"state":"S"}
        foreign={"pid":200,"parent":999,"starttime":99,"state":"S"}
        ledger=DomainLedger.__new__(DomainLedger)
        ledger.ordinal=0;ledger.direct={};ledger.observed={}
        ledger.failed=False;ledger.lastCensus=[]
        with patch("domain_ledger.os.getpid",return_value=100), \
                patch("domain_ledger.task_children",return_value={200}), \
                patch("domain_ledger.census_identity",side_effect=[parent,foreign,parent]), \
                patch("domain_ledger.os.pidfd_open") as opened:
            with self.assertRaisesRegex(RuntimeError,"causal parent edge"):
                ledger.checkpoint()
            opened.assert_not_called()
            self.assertTrue(ledger.failed)
            self.assertEqual(ledger.observed,{})

    def test_parent_incarnation_replaced(self):
        parent={"pid":100,"parent":1,"starttime":10,"state":"S"}
        child={"pid":200,"parent":100,"starttime":20,"state":"S"}
        with patch("domain_ledger.census_identity",return_value={**parent,"starttime":11}):
            with self.assertRaisesRegex(RuntimeError,"causal parent edge"):
                DomainLedger.check_edge(parent,child)

    def test_current_adopted_edge(self):
        parent={"pid":100,"parent":1,"starttime":10,"state":"S"}
        child={"pid":200,"parent":100,"starttime":20,"state":"S"}
        with patch("domain_ledger.census_identity",return_value=parent):
            DomainLedger.check_edge(parent,child)

if __name__=="__main__":unittest.main()
