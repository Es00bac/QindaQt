#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual final driver admission rejects incomplete/foreign/forced retirement."""
import copy,os,unittest
from processes import admit_driver

def positive():
    rows=[]
    for i,app in enumerate(['app-a','app-b']):
        rows.append({'app':app,'pid':100+i,'starttime':1+i,'prefix':'/fixture/'+app+'/prefix',
            'prefixIdentity':[1,i+1,os.getuid(),1],'serverDirectory':[2,i+1,os.getuid(),1],
            'lock':[3,i+1,os.getuid(),1],'socket':[4,i+1,os.getuid(),1],
            'peerPid':100+i,'peerUid':os.getuid()})
    value={'passed':True,'deadlineQualified':True,'clients':[{'application':r['app'],'fixedProgram':'notepad.exe' if i==0 else 'wordpad.exe',
        'normalWindowType':True,'fixedProgramArgumentObserved':True,'xid':str(i+1),
        'xresLocalPid':10+i,'prefix':r['prefix'],'starttime':10+i} for i,r in enumerate(rows)],
        'steps':[{'stage':'type-'+letter,'clipboardText':'qinda '+letter+' 3108',
          'focusWithinClient':True,'beforeSHA256':'a'*64,'afterSHA256':'b'*64,
          'pixelBytes':69120} for letter in ['a','b']]+
          [{'stage':s} for s in ['resize-a','close-a','resize-b','close-b']],
        'serverReadiness':rows,'serverRetirement':[{'app':r['app'],'pid':r['pid'],'starttime':r['starttime'],
            'prefix':r['prefix'],'qualified':True,'exit':0,'pidfdDead':True,'reaped':True,
            'signal':'SIGINT','heldLockReleased':True,'currentLockSame':True,'replacementOwnerAbsent':True} for r in rows],
        'appLaunch':[{'app':r['app'],'pid':10+i,'starttime':10+i} for i,r in enumerate(rows)],
        'appRetirement':[{'app':r['app'],'pid':10+i,'starttime':10+i,'exit':0,'reaped':True,'pidfdDead':True} for i,r in enumerate(rows)],
        'childrenRetirement':{'qualified':True,'kernelECHILDRequired':True,'perPrefixDomains':2,'adoptedSignals':0,'unknownSurvivors':False},'subreaperChecked':True}
    value['lifecycleDomains']=[{'domain':app,'nonce':str(i)*64,
        'supervisor':{'pid':200+i,'starttime':30+i,'parent':1},'qualified':True,
        'supervisorExit':0,'supervisorReaped':True,'supervisorPidfdDead':True,
        'retirement':{'qualified':True,'server':dict(value['serverRetirement'][i]),
                     'app':dict(value['appRetirement'][i]),
                     'children':{'qualified':True,'subreaperChecked':True,
                                 'directReaped':2,'kernelECHILD':True,'adoptedSignals':0}}}
        for i,app in enumerate(['app-a','app-b'])]
    return value
class Admission(unittest.TestCase):
    def test_domain_missing_replayed_or_mixed(self):
        for change in ["missing","nonce","owner","kernel","signal","server","app"]:
            v=positive()
            if change=="missing":v.pop("lifecycleDomains")
            else:
                d=v["lifecycleDomains"][1]
                if change=="nonce":d["nonce"]=v["lifecycleDomains"][0]["nonce"]
                elif change=="owner":d["supervisor"]=v["lifecycleDomains"][0]["supervisor"]
                elif change=="kernel":d["retirement"]["children"]["kernelECHILD"]=False
                elif change=="signal":d["retirement"]["children"]["adoptedSignals"]=1
                elif change=="server":d["retirement"]["server"]["pid"]+=1
                else:d["retirement"]["app"]["starttime"]+=1
            with self.subTest(change=change):
                with self.assertRaises(RuntimeError):admit_driver(v)
    def test_deadline_fact_required(self):
        v=positive();v.pop('deadlineQualified')
        with self.assertRaises(RuntimeError):admit_driver(v)
    def test_expired_fact_refused(self):
        v=positive();v['deadlineExpired']=True
        with self.assertRaises(RuntimeError):admit_driver(v)
    def test_complete_positive(self):self.assertTrue(admit_driver(positive()))
    def test_old_cleanup_receipt_insufficient(self):
        v=positive();v.pop('serverRetirement');v['prefixServerStop']=[{'app':'app-a','exit':0},{'app':'app-b','exit':0}]
        with self.assertRaises(RuntimeError):admit_driver(v)
    def test_every_retirement_fact_required(self):
        for field in ['qualified','exit','pidfdDead','reaped','signal','heldLockReleased','currentLockSame','replacementOwnerAbsent']:
            with self.subTest(field=field):
                v=positive();v['serverRetirement'][1].pop(field)
                with self.assertRaises(RuntimeError):admit_driver(v)
    def test_missing_one_server(self):
        v=positive();v['serverRetirement'].pop()
        with self.assertRaises(RuntimeError):admit_driver(v)
    def test_changed_retirement_lifetime(self):
        v=positive();v['serverRetirement'][1]['starttime']+=1
        with self.assertRaises(RuntimeError):admit_driver(v)
    def test_duplicate_initial_objects(self):
        for field in ['pid','prefixIdentity','serverDirectory','lock','socket']:
            with self.subTest(field=field):
                v=positive();v['serverReadiness'][1][field]=copy.deepcopy(v['serverReadiness'][0][field])
                with self.assertRaises(RuntimeError):admit_driver(v)
    def test_peer_or_uid_wrong(self):
        for field in ['peerPid','peerUid']:
            v=positive();v['serverReadiness'][1][field]+=1
            with self.assertRaises(RuntimeError):admit_driver(v)
    def test_forced_shutdown_refused(self):
        v=positive();v['serverRetirement'][1]['signal']='SIGTERM'
        with self.assertRaises(RuntimeError):admit_driver(v)
    def test_unknown_survivor_refused(self):
        v=positive();v['childrenRetirement']['unknownSurvivors']=True
        with self.assertRaises(RuntimeError):admit_driver(v)
    def test_uncertainty_refused_after_windows(self):
        v=positive();v['uncertainCleanup']=['unknown']
        with self.assertRaises(RuntimeError):admit_driver(v)
if __name__=='__main__':unittest.main(verbosity=2)
