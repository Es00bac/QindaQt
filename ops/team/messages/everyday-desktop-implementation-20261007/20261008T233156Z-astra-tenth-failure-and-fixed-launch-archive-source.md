# Tenth actual failure and fixed-app launch deadline source freeze

Exact source: 328fcf22be3ac061d218e51289e449edc13b44b4. No archive or eleventh VM execution authorized/performed by this handoff.

The once-only tenth outer2702038/start11890168 was held, reaped and absent: exit1 after47.223907s. Unit main2702043/start11890174/invocation5c60aea0e3de49e2a467a5f72ba97f36 settled inactive/dead/MainPID0. Six live witnesses confirm CPU4/memory5GiB/swap0/tasks256/affinity0-7/nice10. QEMU2702138/start11890344 exit0, retired; inputVersionsStable and innerCleanupQualified true. ONE intact WINDOWS and ONE intact RESULT line parse; console fstat/sync/flush/drain now crossed in the real guest. Overall remains FAILED: boot completed, Calculator produced positive1280x800 window, then exact Clock launch command exceeded10s. No second app observation in this run and no retry. Eighth/ninth distinct two-window observations remain separate. PrivatePulse ready is not Audio1 integration.

Immutable raw proof .cache/android-tenth-vm-proof.tar.gz:30252bytes SHA256d5a3be476137cedd508d3bd4ff482fb471bd4fbb53b9aa162f9bc701b23ffbfa,16members/15indexedpayloads. Index9243c7f978f2465bf2b441475220b61cfe26c7b369e65be52c4ddb0c61086ef1. Includes raw serial/result/dispatch/live/argv/plan and exact installed stock app_manager.py/IPlatform.py source. Stock launch lines80–96 performs service discovery and several synchronous RPCs; IPlatform.launchApp174–178 uses transact_sync_reply. The timeout cannot prove absence of launch side effects or the specific blocked RPC.

Root authorized exact Calculator/Clock launch tuples to consume only remaining shared190s budget, like existing boot query. Every other command remains10s. No retry, owner/geometry/two-window condition change, or total bound change. Ten injected tests PASS0/0.007s, docs534/strictMkDocs0/diff0. Fake subprocess only, no new guest.

Requested next gate: ONE archive2340 after independent source/argv review. Clone qualified2325/stage, replace only proof/windows.py, retain7295 other records and exact cpio/gzip/source21 pins. Same120unit115inner150observer/4CPU5GiB/no swap/tasks256/0-7/nice10. No QEMU in archive grant. A separately frozen eleventh manifest can only be completed after actual new archive SHA/size exist. Prior nine failures and tenth are immutable.

Rendered/input/resize/close/relaunch, trusted platform/app identity and host Audio1 remain unqualified. Laptop heavy RELEASED. Continue compatible public interaction-boundary source inspection while awaiting exact archive grant.


## plan.json

SHA256 6f76ebef3c709c73f6c6ae223f873a1bdd640eed0ba3bfa9787c1f731772bce5

```
{
  "schema": 1,
  "kind": "fixed-app-launch-budget-one-object-archive-proposal",
  "sourceCommit": "328fcf22be3ac061d218e51289e449edc13b44b4",
  "root": "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z",
  "output": "archive-2340",
  "stageObjects": 7296,
  "changedObject": "proof/windows.py",
  "unchangedOtherObjects": 7295,
  "patchSHA256": "14e0a0b5d9046e49b767c93d5be8ef8ce02a76d664f8a9e77b052fedf0fd483b",
  "driverSHA256": "7edcdb905c8edd3df6a433d72f9af39a0f8860896cdbcb151847b672047ac9db",
  "observerSHA256": "0f9b16f582027fc85b5524f22ac7d061eb3acec9a0209fc80e922ca2e1788204",
  "unitArgv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-diagnostic-archive-20261008T2340Z.service",
    "--wait",
    "--pipe",
    "--collect",
    "--expand-environment=no",
    "--working-directory=/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z",
    "--property=CPUQuota=400%",
    "--property=CPUAffinity=0-7",
    "--property=MemoryMax=5368709120",
    "--property=MemorySwapMax=0",
    "--property=TasksMax=256",
    "--property=Nice=10",
    "--property=RuntimeMaxSec=120",
    "--property=TimeoutStopSec=5",
    "--property=KillMode=control-group",
    "--property=LimitCORE=0",
    "--property=LimitFSIZE=1073741824",
    "--property=UMask=0077",
    "--property=UnsetEnvironment=PYTHONPATH PYTHONHOME LD_PRELOAD LD_LIBRARY_PATH DISPLAY WAYLAND_DISPLAY",
    "--setenv=PATH=/usr/bin:/bin",
    "--setenv=LANG=C.UTF-8",
    "--setenv=PYTHONDONTWRITEBYTECODE=1",
    "--setenv=DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/qindaqt-android-stage-session",
    "--setenv=DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/qindaqt-android-stage-system",
    "/usr/bin/python3",
    "-B",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/diagnostic-inputs-2340/archive-delta.py"
  ],
  "stagePins": {
    "plan.json": "ed55d32ac2b6da9288c69a612dba364b39a79b3d9f01d44f60e7502bc3a564e3",
    "stage-result.json": "7c090cc5c3ab0a5291c92d873c2ceb93661f09d99b2c1c04bcfd550565a46d83",
    "archive-2325/diagnostic-inventory.json": "3d29d08e52821436e2e063a7a2865fdf4228fc7662580896c2e28070f37c9212",
    "archive-2325/diagnostic-plan.json": "d96d0927c1c7554d9400c219abceb687189f0607fb8fe63bff3fa1362e11c48f"
  },
  "runtimeAuthorized": false,
  "bounds": {
    "unit": 120,
    "inner": 115,
    "observer": 150,
    "cpu": 4,
    "memoryBytes": 5368709120,
    "swap": 0,
    "tasks": 256,
    "affinity": "0-7",
    "nice": 10
  },
  "baseStage": "archive-2325/stage"
}
```

## archive-delta.py

SHA256 7edcdb905c8edd3df6a433d72f9af39a0f8860896cdbcb151847b672047ac9db

```
"""Execute only the manager-admitted existing boot_plan/cpio/gzip sequence."""
import hashlib,json,os,stat,subprocess,sys,time,shutil
from pathlib import Path
ROOT=Path('/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z')
OUT=ROOT/'archive-2340'
PATCH=ROOT/'diagnostic-inputs-2340/windows.py'
PATCH_SHA='14e0a0b5d9046e49b767c93d5be8ef8ce02a76d664f8a9e77b052fedf0fd483b'
PLAN='ed55d32ac2b6da9288c69a612dba364b39a79b3d9f01d44f60e7502bc3a564e3'
PINS={'archive-2325/diagnostic-inventory.json':'3d29d08e52821436e2e063a7a2865fdf4228fc7662580896c2e28070f37c9212','archive-2325/diagnostic-plan.json':'d96d0927c1c7554d9400c219abceb687189f0607fb8fe63bff3fa1362e11c48f','stage-result.json':'7c090cc5c3ab0a5291c92d873c2ceb93661f09d99b2c1c04bcfd550565a46d83'}
TOOLS={'/usr/bin/cpio':'c2fb928222355ae6883ceb06d71da73d70e80b40fcd0d00aae4c73f463c4e521','/usr/bin/gzip':'a4a9164df114a27c0a73f113a88a9c9ddb3b2fe197af0dde2ea2649fa1691ccd'}
def digest(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def protected(p,expected):
    s=p.lstat()
    assert stat.S_ISREG(s.st_mode) and s.st_uid==0 and not s.st_mode&0o222
    assert digest(p)==expected
def pins():
    assert ROOT.lstat().st_uid==0 and stat.S_IMODE(ROOT.lstat().st_mode)==0o700
    protected(ROOT/'inputs/plan.json',PLAN)
    protected(PATCH,PATCH_SHA)
    plan=json.loads((ROOT/'inputs/plan.json').read_text())
    assert len(plan['sourcePins'])==21
    for name,sha in plan['sourcePins'].items():protected(ROOT/'inputs/source'/Path(name).name,sha)
    for name,sha in PINS.items():
        p=ROOT/name;s=p.lstat()
        assert stat.S_ISREG(s.st_mode) and s.st_uid==0 and not s.st_mode&0o022
        assert digest(p)==sha
    for name,sha in TOOLS.items():
        p=Path(name).resolve();s=p.stat()
        assert stat.S_ISREG(s.st_mode) and s.st_uid==0 and not s.st_mode&0o022
        assert digest(p)==sha
pins() # Protected source bytes checked before importing the unchanged reader.
sys.path.insert(0,str(ROOT/'inputs/source'))
from boot_plan import plan
from run_vm import envelope
limits=envelope('laptop')
OUT.mkdir(mode=0o700)
result={'success':False,'runtimeAuthorized':False,'envelope':limits,'phases':[]}
start=time.monotonic();primary=None
env={'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','HOME':'/nonexistent','DBUS_SESSION_BUS_ADDRESS':'unix:path=/nonexistent','DBUS_SYSTEM_BUS_ADDRESS':'unix:path=/nonexistent'}
def phase(name,argv,cwd,timeout,stdin=None,stdout=None):
    row={'name':name,'argv':argv,'cwd':str(cwd)};result['phases'].append(row)
    with (OUT/(name+'.stderr')).open('xb') as err:
        began=time.monotonic()
        p=subprocess.run(argv,cwd=cwd,env=env,stdin=stdin or subprocess.DEVNULL,
                         stdout=stdout or subprocess.DEVNULL,stderr=err,
                         timeout=min(timeout,max(.001,115-(time.monotonic()-start))),close_fds=True)
        row.update(exit=p.returncode,elapsedSeconds=time.monotonic()-began)
        if p.returncode:raise RuntimeError(name+'-failed')
try:
    inv=json.loads((ROOT/'archive-2325/diagnostic-inventory.json').read_text())
    before=plan(ROOT/'archive-2325/stage',inv)
    assert before==json.loads((ROOT/'archive-2325/diagnostic-plan.json').read_text())
    clone=OUT/'stage'
    shutil.copytree(ROOT/'archive-2325/stage',clone,symlinks=True,copy_function=shutil.copy)
    copied=plan(clone,inv)
    assert dict(copied,cwd=before['cwd'])==before
    target=clone/'proof/windows.py'; info=target.lstat()
    assert stat.S_ISREG(info.st_mode) and info.st_nlink==1 and info.st_uid==0
    assert digest(target)==inv['objects']['proof/windows.py']['sha256']
    target.unlink()
    with PATCH.open('rb') as src,target.open('xb') as dst:
        shutil.copyfileobj(src,dst,1024*1024);dst.flush();os.fsync(dst.fileno())
    target.chmod(stat.S_IMODE(info.st_mode))
    assert digest(target)==PATCH_SHA
    changed=json.loads(json.dumps(inv))
    changed['objects']['proof/windows.py'].update(size=target.stat().st_size,sha256=PATCH_SHA)
    assert len(changed['objects'])==7296
    assert [k for k in inv['objects'] if inv['objects'][k]!=changed['objects'][k]]==['proof/windows.py']
    archive=plan(clone,changed)
    (OUT/'diagnostic-inventory.json').write_text(json.dumps(changed,indent=2)+'\n')
    (OUT/'diagnostic-plan.json').write_text(json.dumps(archive,indent=2)+'\n')
    (OUT/'before-plan.json').write_text(json.dumps(before,indent=2)+'\n')
    with (OUT/'paths.nul').open('xb') as f:
        for name in archive['stdinPaths']:f.write(name.encode()+b'\0')
        f.flush();os.fsync(f.fileno())
    with (OUT/'paths.nul').open('rb') as src,(OUT/'initramfs.cpio').open('xb') as dst:
        phase('cpio',archive['cpioArgv'],clone,40,src,dst)
        dst.flush();os.fsync(dst.fileno())
    with (OUT/'initramfs.cpio.gz').open('xb') as dst:
        phase('gzip',archive['gzipArgv']+['-c','initramfs.cpio'],OUT,60,stdout=dst)
        dst.flush();os.fsync(dst.fileno())
    phase('gzip-test',['/usr/bin/gzip','-t','initramfs.cpio.gz'],OUT,10)
finally:
    primary=sys.exc_info()[1]
    try:
        after=plan(ROOT/'archive-2325/stage',json.loads((ROOT/'archive-2325/diagnostic-inventory.json').read_text()))
        result['inventoryStable']=after==json.loads((ROOT/'archive-2325/diagnostic-plan.json').read_text())
        if 'archive' in globals():
            result['diagnosticInventoryStable']=plan(clone,changed)==archive
        pins();result['pinsStable']=True
        result['outputs']={}
        for name in ('initramfs.cpio','initramfs.cpio.gz'):
            p=OUT/name
            if p.exists():
                assert 0<p.stat().st_size<1024**3
                p.chmod(0o400)
                result['outputs'][name]={'size':p.stat().st_size,'sha256':digest(p)}
        result['success']=primary is None and result['inventoryStable'] and result.get('diagnosticInventoryStable') is True and len(result['phases'])==3 and time.monotonic()-start<115
    except BaseException as error:
        result['finalEvidenceError']=type(error).__name__
        if primary is None:raise
    finally:
        result['elapsedSeconds']=time.monotonic()-start
        if primary:result['errorType']=type(primary).__name__
        (OUT/'archive-result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result))
if not result['success'] or time.monotonic()-start>=115:raise SystemExit(1)
```

## run-once.py

SHA256 0f9b16f582027fc85b5524f22ac7d061eb3acec9a0209fc80e922ca2e1788204

```
import json,os,subprocess,time
from pathlib import Path
B=Path('/home/cabewse/work_space/container-wm/.cache/android-launch-archive-20261008')
plan=json.loads((B/'plan.json').read_text())
unit='qindaqt-android-diagnostic-archive-20261008T2340Z.service';argv=plan['unitArgv']
(B/'argv.json').write_text(json.dumps(argv,indent=2)+'\n')
def stat(pid):
 text=Path('/proc',str(pid),'stat').read_text();fields=text[text.rfind(')')+2:].split()
 return {'pid':pid,'starttime':fields[19],'nice':fields[16],'state':fields[0]}
result={'argv':argv,'unit':unit,'live':[]};start=time.monotonic();p=None;pidfd=None
try:
 with (B/'unit.log').open('xb') as log:
  p=subprocess.Popen(argv,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,close_fds=True)
  pidfd=os.pidfd_open(p.pid);result['outer']=stat(p.pid)
  while p.poll() is None:
   if time.monotonic()-start>150:raise TimeoutError('outer-unit-deadline')
   r=subprocess.run(['systemctl','show',unit,'--no-pager','--property=MainPID,InvocationID,ControlGroup,ActiveState,SubState,Result,CPUQuotaPerSecUSec,MemoryMax,MemorySwapMax,TasksMax,Nice,CPUAffinity,RuntimeMaxUSec,TimeoutStopUSec'],capture_output=True,text=True,timeout=3)
   values=dict(line.split('=',1) for line in r.stdout.splitlines() if '=' in line)
   pid=int(values.get('MainPID','0'))
   if pid and len(result['live'])<6:
    try:
     live={'unit':values,'process':stat(pid),'status':Path('/proc',str(pid),'status').read_text(),'cgroup':Path('/proc',str(pid),'cgroup').read_text()}
     cg=Path('/sys/fs/cgroup')/values['ControlGroup'].lstrip('/')
     live['kernel']={n:(cg/n).read_text() for n in ('cpu.max','memory.max','memory.swap.max','pids.max')}
     result['live'].append(live)
    except FileNotFoundError:pass
   if (B/'unit.log').stat().st_size>2*1024**2:raise RuntimeError('output-bound')
   time.sleep(.05)
  result['exit']=p.wait(timeout=1)
 result['outerReaped']=True;result['outerProcAbsent']=not Path('/proc',str(p.pid)).exists()
 r=subprocess.run(['systemctl','show',unit,'--no-pager','--property=MainPID,InvocationID,ActiveState,SubState,Result'],capture_output=True,text=True,timeout=3)
 result['finalUnit']={'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr}
except BaseException as e:
 result['exception']=type(e).__name__;raise
finally:
 if pidfd is not None:os.close(pidfd)
 result['elapsedSeconds']=time.monotonic()-start
 (B/'dispatch-result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'exit':result.get('exit'),'liveWitnesses':len(result['live']),'elapsedSeconds':result['elapsedSeconds'],'outerReaped':result.get('outerReaped')}))
```

## argv.json

SHA256 856489a0f0f4e77b3d286151346c3515419bf16aa7d1470ec8cf991d701eb019

```
[
  "sudo",
  "-n",
  "systemd-run",
  "--unit=qindaqt-android-diagnostic-archive-20261008T2340Z.service",
  "--wait",
  "--pipe",
  "--collect",
  "--expand-environment=no",
  "--working-directory=/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z",
  "--property=CPUQuota=400%",
  "--property=CPUAffinity=0-7",
  "--property=MemoryMax=5368709120",
  "--property=MemorySwapMax=0",
  "--property=TasksMax=256",
  "--property=Nice=10",
  "--property=RuntimeMaxSec=120",
  "--property=TimeoutStopSec=5",
  "--property=KillMode=control-group",
  "--property=LimitCORE=0",
  "--property=LimitFSIZE=1073741824",
  "--property=UMask=0077",
  "--property=UnsetEnvironment=PYTHONPATH PYTHONHOME LD_PRELOAD LD_LIBRARY_PATH DISPLAY WAYLAND_DISPLAY",
  "--setenv=PATH=/usr/bin:/bin",
  "--setenv=LANG=C.UTF-8",
  "--setenv=PYTHONDONTWRITEBYTECODE=1",
  "--setenv=DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/qindaqt-android-stage-session",
  "--setenv=DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent/qindaqt-android-stage-system",
  "/usr/bin/python3",
  "-B",
  "/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z/diagnostic-inputs-2340/archive-delta.py"
]
```

## mechanical-diff.txt

SHA256 07907e5ee5a780fa4fa4881e3200e3972a0ff6c9a459bfb7831f232fcc9226f6

```
--- accepted2325
+++ proposed2340
@@ -2,11 +2,11 @@
 import hashlib,json,os,stat,subprocess,sys,time,shutil
 from pathlib import Path
 ROOT=Path('/var/tmp/qindaqt-android-laptop-stage-20261008T2250Z')
-OUT=ROOT/'archive-2325'
-PATCH=ROOT/'diagnostic-inputs-2325/guest.py'
-PATCH_SHA='d8a9e638158de6208bcec8b293da5a0f9ccf4efb691978f8e95bcbfb1ed0b512'
+OUT=ROOT/'archive-2340'
+PATCH=ROOT/'diagnostic-inputs-2340/windows.py'
+PATCH_SHA='14e0a0b5d9046e49b767c93d5be8ef8ce02a76d664f8a9e77b052fedf0fd483b'
 PLAN='ed55d32ac2b6da9288c69a612dba364b39a79b3d9f01d44f60e7502bc3a564e3'
-PINS={'archive-2315/diagnostic-inventory.json':'653add3e6daea914102a6f529deaf67d4b6ce8aa83a0719114438d133cfc52fe','archive-2315/diagnostic-plan.json':'dbf0959b1f5b41b3c0cb3c34b65b516f50c681586b1abc74f10a5d58d45cc7a5','stage-result.json':'7c090cc5c3ab0a5291c92d873c2ceb93661f09d99b2c1c04bcfd550565a46d83'}
+PINS={'archive-2325/diagnostic-inventory.json':'3d29d08e52821436e2e063a7a2865fdf4228fc7662580896c2e28070f37c9212','archive-2325/diagnostic-plan.json':'d96d0927c1c7554d9400c219abceb687189f0607fb8fe63bff3fa1362e11c48f','stage-result.json':'7c090cc5c3ab0a5291c92d873c2ceb93661f09d99b2c1c04bcfd550565a46d83'}
 TOOLS={'/usr/bin/cpio':'c2fb928222355ae6883ceb06d71da73d70e80b40fcd0d00aae4c73f463c4e521','/usr/bin/gzip':'a4a9164df114a27c0a73f113a88a9c9ddb3b2fe197af0dde2ea2649fa1691ccd'}
 def digest(p):
     with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
@@ -48,25 +48,25 @@
         row.update(exit=p.returncode,elapsedSeconds=time.monotonic()-began)
         if p.returncode:raise RuntimeError(name+'-failed')
 try:
-    inv=json.loads((ROOT/'archive-2315/diagnostic-inventory.json').read_text())
-    before=plan(ROOT/'archive-2315/stage',inv)
-    assert before==json.loads((ROOT/'archive-2315/diagnostic-plan.json').read_text())
+    inv=json.loads((ROOT/'archive-2325/diagnostic-inventory.json').read_text())
+    before=plan(ROOT/'archive-2325/stage',inv)
+    assert before==json.loads((ROOT/'archive-2325/diagnostic-plan.json').read_text())
     clone=OUT/'stage'
-    shutil.copytree(ROOT/'archive-2315/stage',clone,symlinks=True,copy_function=shutil.copy)
+    shutil.copytree(ROOT/'archive-2325/stage',clone,symlinks=True,copy_function=shutil.copy)
     copied=plan(clone,inv)
     assert dict(copied,cwd=before['cwd'])==before
-    target=clone/'proof/guest.py'; info=target.lstat()
+    target=clone/'proof/windows.py'; info=target.lstat()
     assert stat.S_ISREG(info.st_mode) and info.st_nlink==1 and info.st_uid==0
-    assert digest(target)==inv['objects']['proof/guest.py']['sha256']
+    assert digest(target)==inv['objects']['proof/windows.py']['sha256']
     target.unlink()
     with PATCH.open('rb') as src,target.open('xb') as dst:
         shutil.copyfileobj(src,dst,1024*1024);dst.flush();os.fsync(dst.fileno())
     target.chmod(stat.S_IMODE(info.st_mode))
     assert digest(target)==PATCH_SHA
     changed=json.loads(json.dumps(inv))
-    changed['objects']['proof/guest.py'].update(size=target.stat().st_size,sha256=PATCH_SHA)
+    changed['objects']['proof/windows.py'].update(size=target.stat().st_size,sha256=PATCH_SHA)
     assert len(changed['objects'])==7296
-    assert [k for k in inv['objects'] if inv['objects'][k]!=changed['objects'][k]]==['proof/guest.py']
+    assert [k for k in inv['objects'] if inv['objects'][k]!=changed['objects'][k]]==['proof/windows.py']
     archive=plan(clone,changed)
     (OUT/'diagnostic-inventory.json').write_text(json.dumps(changed,indent=2)+'\n')
     (OUT/'diagnostic-plan.json').write_text(json.dumps(archive,indent=2)+'\n')
@@ -84,8 +84,8 @@
 finally:
     primary=sys.exc_info()[1]
     try:
-        after=plan(ROOT/'archive-2315/stage',json.loads((ROOT/'archive-2315/diagnostic-inventory.json').read_text()))
-        result['inventoryStable']=after==json.loads((ROOT/'archive-2315/diagnostic-plan.json').read_text())
+        after=plan(ROOT/'archive-2325/stage',json.loads((ROOT/'archive-2325/diagnostic-inventory.json').read_text()))
+        result['inventoryStable']=after==json.loads((ROOT/'archive-2325/diagnostic-plan.json').read_text())
         if 'archive' in globals():
             result['diagnosticInventoryStable']=plan(clone,changed)==archive
         pins();result['pinsStable']=True
```
