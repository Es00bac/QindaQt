# Diagnostic archive delta — source only, execution pending

Root accepted guest source9dec1e931 and authorized this plan only. First failed VM proof698c7c and original archive3542cc remain unchanged.

Use existing qualified archive driver with fresh archive-2145 output and private cloned stage. It first rechecks original protected20 source pins, stage inventory/archive plan/result and installed cpio/gzip bytes. Original7208-object admission precedes copying. The clone preserves symlinks and modes, independently copies regular files, and must pass the same complete original inventory. Only cloned proof/guest.py is then replaced with protected accepted9dec bytes (single-link/current old hash checked). Exact expected inventory changes only that entry's size/hash; all other7207 records and all7208 modes/kinds remain unchanged. Existing boot_plan/cpio/gzip/gzip-test run unchanged over clone. Final checks revalidate original stage, modified clone, all source/tool pins and outputs. Original stage/archive are never written.

Proposed protection: root-owned0700 diagnostic-inputs-2145 under existing protected2050 root, only reviewed guest.py and archive-delta.py copied0400; independent hash checks before Python imports. Plan SHA256c8338d87626407feae2754f869d292feccc79f33ecd422bff3da270e71797620; driver756e037eee4b34ad09da8159a9b19a588cdab3db29bc48e9a706d003923fd70a; guest74fdea753c8c90f1b213750597e0e075af9cbc65b85b0605b01754b5bf8ef46f.

Same120s/4CPU/5GiB/swap0/tasks256/nice10/affinity0-7 unit;115s internal admission, inherited outer150s held-process observation. No package, installation, VM or new supervisor. All partial output retained. Source AST/hash checks only; archive not executed. Requested next action: manager exact source/argv admission then one archive execution once Power releases heavy, followed by review of actual fresh fourth input before a separately granted diagnostic VM.

## Exact plan
~~~json
{
  "schema": 1,
  "source": "9dec1e9315ec368855f42813754edbbcbe41c790",
  "executed": false,
  "root": "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z",
  "freshOutput": "archive-2145",
  "protectedInputs": "diagnostic-inputs-2145",
  "guestSHA256": "74fdea753c8c90f1b213750597e0e075af9cbc65b85b0605b01754b5bf8ef46f",
  "scriptSHA256": "756e037eee4b34ad09da8159a9b19a588cdab3db29bc48e9a706d003923fd70a",
  "originalArchiveSHA256": "3542cc25515b1016e26f60f206f4f767eab54c764ab19dba11ba32b4fdb8ee46",
  "onlyChangedStageObject": "proof/guest.py",
  "stageObjectCount": 7208,
  "resourcePolicy": "same admitted120s/4CPU/5GiB/swap0/tasks256/nice10/affinity0-7 archive envelope; no guest",
  "argv": [
    "/usr/bin/python3",
    "-B",
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2145/archive-delta.py"
  ],
  "unitArgv": [
    "sudo",
    "-n",
    "systemd-run",
    "--unit=qindaqt-android-diagnostic-archive-20261008T2145Z.service",
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
    "/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z/diagnostic-inputs-2145/archive-delta.py"
  ]
}
~~~

## Exact driver
~~~python
"""Execute only the manager-admitted existing boot_plan/cpio/gzip sequence."""
import hashlib,json,os,stat,subprocess,sys,time,shutil
from pathlib import Path
ROOT=Path('/var/tmp/qindaqt-android-laptop-stage-20261008T2050Z')
OUT=ROOT/'archive-2145'
PATCH=ROOT/'diagnostic-inputs-2145/guest.py'
PATCH_SHA='74fdea753c8c90f1b213750597e0e075af9cbc65b85b0605b01754b5bf8ef46f'
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
