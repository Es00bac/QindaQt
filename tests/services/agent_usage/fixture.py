#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
import json, os, sys, time
mode=os.environ.get("QINDAQT_USAGE_FIXTURE","normal")
if mode=="timeout":
    time.sleep(10)
elif mode=="fail":
    sys.exit(8)
elif mode=="overflow":
    sys.stdout.write("x"*70000);sys.stdout.flush();time.sleep(10)
else:
    for line in sys.stdin:
        value=json.loads(line)
        method=value["method"]
        if method not in ("initialize","initialized","account/rateLimits/read","account/usage/read"):
            sys.exit(9)
        if method=="initialized": continue
        if (method=="account/rateLimits/read" and mode=="limits-unsupported") or (method=="account/usage/read" and mode=="unsupported"):
            print(json.dumps({"id":value["id"],"error":{"code":-32601,"message":"private-do-not-copy"}}),flush=True)
            continue
        if method=="initialize": result={}
        elif method=="account/rateLimits/read":
            result={"rateLimitsByLimitId":{"codex":{"primary":{"usedPercent":25,"resetsAt":2000000000}}}}
        elif mode=="unsupported":
            print(json.dumps({"id":value["id"],"error":{"code":-32601,"message":"private-do-not-copy"}}),flush=True)
            continue
        else: result={"summary":{"lifetimeTokens":1234}}
        print(json.dumps({"id":value["id"],"result":result}),flush=True)
