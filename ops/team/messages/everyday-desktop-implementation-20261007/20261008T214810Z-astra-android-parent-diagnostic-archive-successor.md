# Guest parent repair and diagnostic archive — exact source successor

Runtime source beb2f4dc13cecc3910357547759c7870cfaf4b10 adds only guest Path("/var/lib").mkdir(mode=0o755, exist_ok=True) and two explanatory comments atop accepted9dec logging flags. It runs after unexpected existing-runtime refusal, before private bus/stock calls. /var is the fresh fixture tmpfs; no host path is adopted. Pinned stock tools/__init__.py calls os.mkdir(/var/lib/waydroid) before logging, so missing parent is a concrete source inference, not yet a reproduced underlying exception. Original failed boot/guest cleanup remains failed.

Fresh archive-2150/diagnostic-inputs-2150 plan replaces the unexecuted2145 proposal. Only guest.py input hash and freshnames change in its archive driver; same7208-object before/after admission, one guest file difference, allother7207 records unchanged, no package inputs changed. Original qualified stage/archive untouched, all failures retained. Same120s/4CPU/5GiB/swap0/tasks256/halfcore0-7/nice10 envelope and existing held observer150s. No archive/guest action executed.

Exact hashes:
- guest.py: f946b405dc8b4706626700d967fbe4f5924b3a0aee9e7aa1f56a4ee7ab9d6710
- archive-delta.py: 3e9f24f324b6e3c8f4a6c4b91b77559200380a07cb04ead8d36bedea533cd776
- plan.json: 4140d3e2cad49b4b64df988b484ee4aecc75e8a10abca1ae3fe2e89461757e93
- run-once.py: 7dfa82464d177cbbce7d5d937d9cc7d23c273705b372b953493e0cef391aa6f0

## Exact argv/plan
~~~json
{
  "schema": 1,
  "source": "beb2f4dc13cecc3910357547759c7870cfaf4b10",
  "executed": false,
  "root": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z",
  "freshOutput": "archive-2150",
  "protectedInputs": "diagnostic-inputs-2150",
  "guestSHA256": "f946b405dc8b4706626700d967fbe4f5924b3a0aee9e7aa1f56a4ee7ab9d6710",
  "scriptSHA256": "3e9f24f324b6e3c8f4a6c4b91b77559200380a07cb04ead8d36bedea533cd776",
  "originalArchiveSHA256": "3542cc25515b1016e26f60f206f4f767eab54c764ab19dba11ba32b4fdb8ee46",
  "onlyChangedStageObject": "proof/guest.py",
  "stageObjectCount": 7208,
  "resourcePolicy": "same admitted120s/4CPU/5GiB/swap0/tasks256/nice10/affinity0-7 archive envelope; no guest",
  "argv": [
    "/usr/bin/python3",
    "-B",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2150/archive-delta.py"
  ],
  "unitArgv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-diagnostic-archive-20261008T2150Z.service",
    "--wait",
    "--pipe",
    "--collect",
    "--expand-environment=no",
    "--working-directory=/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z",
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
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2150/archive-delta.py"
  ]
}
~~~

## Exact archive driver
~~~python
"""Execute only the manager-admitted existing boot_plan/cpio/gzip sequence."""
import hashlib,json,os,stat,subprocess,sys,time,shutil
from pathlib import Path
ROOT=Path('/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z')
OUT=ROOT/'archive-2150'
PATCH=ROOT/'diagnostic-inputs-2150/guest.py'
PATCH_SHA='f946b405dc8b4706626700d967fbe4f5924b3a0aee9e7aa1f56a4ee7ab9d6710'
PLAN='665b3988a3773b0531fa8d8a328d4cb938304eca71bf3ac34b574b7161ac8f61'
PINS={'stage-inventory.json':'1dea8d37a245548caf3030a4847ed8330f3171a157b3f26af6a467aa9e6f1214','archive-plan.json':'f9a1e59f98ab6299cba35de77ccc073e236962b3b07bb205ff52558af87de6bb','stage-result.json':'a0f65fb2cedd1729a4c53f7bfcd1952c9280d8ce9fd066fec0be0315870f968d'}
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
    assert len(plan['sourcePins'])==20
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
    inv=json.loads((ROOT/'stage-inventory.json').read_text())
    before=plan(ROOT/'stage',inv)
    assert before==json.loads((ROOT/'archive-plan.json').read_text())
    clone=OUT/'stage'
    shutil.copytree(ROOT/'stage',clone,symlinks=True,copy_function=shutil.copy)
    copied=plan(clone,inv)
    assert dict(copied,cwd=before['cwd'])==before
    target=clone/'proof/guest.py'; info=target.lstat()
    assert stat.S_ISREG(info.st_mode) and info.st_nlink==1 and info.st_uid==0
    assert digest(target)==inv['objects']['proof/guest.py']['sha256']
    target.unlink()
    with PATCH.open('rb') as src,target.open('xb') as dst:
        shutil.copyfileobj(src,dst,1024*1024);dst.flush();os.fsync(dst.fileno())
    target.chmod(stat.S_IMODE(info.st_mode))
    assert digest(target)==PATCH_SHA
    changed=json.loads(json.dumps(inv))
    changed['objects']['proof/guest.py'].update(size=target.stat().st_size,sha256=PATCH_SHA)
    assert len(changed['objects'])==7208
    assert [k for k in inv['objects'] if inv['objects'][k]!=changed['objects'][k]]==['proof/guest.py']
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
        after=plan(ROOT/'stage',json.loads((ROOT/'stage-inventory.json').read_text()))
        result['inventoryStable']=after==json.loads((ROOT/'archive-plan.json').read_text())
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
~~~

## Exact observer
~~~python
import json,os,subprocess,time
from pathlib import Path
B=Path('/home/cabewse/work_space/container-wm/.cache/android-parent-diagnostic-archive-20261008')
plan=json.loads((B/'plan.json').read_text())
unit='qindaqt-android-diagnostic-archive-20261008T2150Z.service';argv=plan['unitArgv']
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
~~~
