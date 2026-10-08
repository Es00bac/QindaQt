# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded private inherited-FD generation/sequence receipts, not a public daemon."""
import json,select,time
MAX_PACKET=65536
def require_time(deadline,clock=time.monotonic):
    remaining=deadline-clock()
    if remaining<=0:raise RuntimeError("domain protocol deadline")
    return remaining
def encode(value):
    data=json.dumps(value,separators=(",",":"),allow_nan=False).encode()
    if len(data)>MAX_PACKET:raise RuntimeError("domain packet bound")
    return data
def receive(sock,deadline):
    if not select.select([sock],[],[],require_time(deadline))[0]:
        raise RuntimeError("domain control deadline")
    data=sock.recv(MAX_PACKET+1)
    require_time(deadline)
    if not data:raise RuntimeError("domain control EOF")
    if len(data)>MAX_PACKET:raise RuntimeError("domain packet bound")
    try:return json.loads(data,parse_constant=lambda value:(_ for _ in ()).throw(ValueError()))
    except (ValueError,UnicodeError):raise RuntimeError("domain packet malformed")
def send(sock,value,deadline):
    data=encode(value)
    if not select.select([],[sock],[],require_time(deadline))[1]:
        raise RuntimeError("domain control deadline")
    if sock.send(data)!=len(data):raise RuntimeError("domain short packet")
    require_time(deadline)
def request(value,name,nonce,sequence):
    if not isinstance(value,dict) or set(value)!={"domain","nonce","sequence","operation"}:
        raise RuntimeError("domain request schema")
    if type(value["sequence"]) is not int or value["domain"]!=name or value["nonce"]!=nonce or value["sequence"]!=sequence:
        raise RuntimeError("domain request generation/replay")
    if value["operation"] not in {"checkpoint","retire","finish"}:
        raise RuntimeError("domain operation refused")
    return value["operation"]
def receipt(value,name,nonce,sequence,initial):
    if not isinstance(value,dict) or set(value)!={"domain","nonce","sequence","supervisor","passed","payload"}:
        raise RuntimeError("domain receipt schema")
    if type(value["sequence"]) is not int or value["domain"]!=name or value["nonce"]!=nonce or value["sequence"]!=sequence or value["supervisor"]!=initial:
        raise RuntimeError("domain receipt lifetime/generation/replay")
    if value["passed"] is not True or not isinstance(value["payload"],dict):
        raise RuntimeError("domain refused operation")
    return value["payload"]
