#!/usr/bin/env python3
"""Dev-only fake QindaLink desktop for exercising the phone engine (never ships).

Listens on 127.0.0.1:47199 (NOT the real service's 47160), speaks TLS 1.3 with a
throwaway P-256 certificate, runs the desktop side of pairing, and prints only
non-personal facts: frame types, battery level and network type.

Live recipe (see phone/README.md):
    adb -s <serial> reverse tcp:47160 tcp:47199
    phone/tools/fake-desktop.py --serial <serial>
    ... accept on the phone; press Enter here (or pass --auto-accept) to accept ...
    adb -s <serial> reverse --remove tcp:47160

Trusting the phone: Python's ssl cannot accept an unknown client certificate
via callback, so the phone's PUBLIC certificate (written by the debug build to
its private files dir) is fetched with `adb run-as` and used as the trust
anchor for each connection. No private key or personal data is involved.
"""
import argparse, hashlib, json, os, secrets, socket, ssl, struct, subprocess, sys, tempfile, threading, time

PORT = 47199
PKG = "org.qindaqt.link"


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, capture_output=True, **kw)


def make_identity(d):
    key, crt = os.path.join(d, "key.pem"), os.path.join(d, "cert.pem")
    run(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", key])
    run(["openssl", "req", "-new", "-x509", "-key", key, "-out", crt, "-days", "30", "-subj", "/CN=fake-qinda-desktop"])
    return key, crt


def spki_digest_of_cert_pem(pem_bytes):
    pub = run(["openssl", "x509", "-pubkey", "-noout"], input=pem_bytes).stdout
    der = run(["openssl", "pkey", "-pubin", "-outform", "DER"], input=pub).stdout
    return hashlib.sha256(der).digest()


def recv_exact(s, n):
    b = b""
    while len(b) < n:
        c = s.recv(n - len(b))
        if not c:
            raise EOFError
        b += c
    return b


def read_frame(s):
    (n,) = struct.unpack(">I", recv_exact(s, 4))
    if not 1 <= n <= 4194304:
        raise ValueError("bad length")
    return json.loads(recv_exact(s, n).decode("utf-8"))


def send_frame(s, env):
    p = json.dumps(env, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    s.sendall(struct.pack(">I", len(p)) + p)


def handle(raw, key, crt, args, pin):
    with tempfile.TemporaryDirectory() as d:
        phone_pem = os.path.join(d, "phone.pem")
        out = run(["adb", "-s", args.serial, "exec-out", "run-as", PKG, "cat", "files/identity-cert.pem"]).stdout
        open(phone_pem, "wb").write(out)
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_3
        ctx.load_cert_chain(crt, key)
        ctx.verify_mode = ssl.CERT_REQUIRED
        ctx.load_verify_locations(phone_pem)
        s = ctx.wrap_socket(raw, server_side=True)
    spki_p = spki_digest_of_cert_pem(ssl.DER_cert_to_PEM_cert(s.getpeercert(binary_form=True)).encode())
    spki_d = spki_digest_of_cert_pem(open(crt, "rb").read())
    pid = spki_p.hex()
    trusted = pin.get("id") == pid
    print(f"tls ok tls={s.version()} phone={pid[:12]}... pinned={trusted}", flush=True)
    send_frame(s, {"t": "hello", "b": {"proto": 1, "role": "desktop", "name": "fake-desktop", "model": "dev", "os": "dev",
                                       "app": "0.0", "caps": ["status"], "trusted": trusted}})
    nd = None
    local_ok = remote_ok = False
    code = ""
    while True:
        f = read_frame(s)
        t, b = f.get("t"), f.get("b", {})
        if t == "hello":
            print(f"phone hello: proto={b.get('proto')} caps={b.get('caps')} trusted={b.get('trusted')}", flush=True)
        elif t == "pair.request":
            nd = secrets.token_bytes(32)
            c = hashlib.sha256(b"QLINK-COMMIT-1" + nd + spki_d + spki_p).hexdigest()
            send_frame(s, {"t": "pair.commit", "re": f["id"], "b": {"c": c}})
        elif t == "pair.nonce":
            np_ = bytes.fromhex(b["np"])
            send_frame(s, {"t": "pair.reveal", "b": {"nd": nd.hex()}})
            h = hashlib.sha256(b"QLINK-SAS-1" + spki_d + spki_p + nd + np_).digest()
            code = "%06d" % (int.from_bytes(h[:4], "big") % 1000000)
            print(f"PAIRING CODE (desktop): {code[:3]} {code[3:]}", flush=True)
            if args.auto_accept:
                time.sleep(args.accept_delay)
            else:
                input("press Enter to accept on the desktop side... ")
            send_frame(s, {"t": "pair.confirm", "b": {"accept": True}})
            local_ok = True
        elif t == "pair.confirm":
            remote_ok = bool(b.get("accept"))
            print(f"phone confirm accept={remote_ok}", flush=True)
        elif t == "status":
            bat, net = b.get("battery", {}), b.get("network", {})
            print(f"status battery={bat.get('level')}% charging={bat.get('charging')} plugged={bat.get('plugged')} network={net.get('type')}", flush=True)
        elif t == "ping":
            send_frame(s, {"t": "pong", "re": f["id"]})
        elif t == "bye":
            print(f"bye {b.get('reason')}", flush=True)
            return
        else:
            print(f"frame {t}", flush=True)
        if local_ok and remote_ok and pin.get("id") != pid:
            pin["id"] = pid
            print(f"PAIRED: pin stored (code was {code[:3]} {code[3:]})", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial", required=True)
    ap.add_argument("--auto-accept", action="store_true", help="accept after --accept-delay seconds instead of waiting for Enter")
    ap.add_argument("--accept-delay", type=float, default=2.0)
    ap.add_argument("--port", type=int, default=PORT)
    args = ap.parse_args()
    d = tempfile.mkdtemp(prefix="fake-desktop-")
    key, crt = make_identity(d)
    pin = {}
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", args.port))
    srv.listen(4)
    print(f"fake desktop on 127.0.0.1:{args.port}", flush=True)
    while True:
        raw, _ = srv.accept()
        threading.Thread(target=lambda r=raw: _guard(r, key, crt, args, pin), daemon=True).start()


def _guard(raw, key, crt, args, pin):
    try:
        handle(raw, key, crt, args, pin)
    except (EOFError, ConnectionError, ssl.SSLError) as e:
        print(f"connection ended: {type(e).__name__} {e}", flush=True)
    except Exception as e:  # dev tool: keep listening
        print(f"error: {type(e).__name__}: {e}", flush=True)


if __name__ == "__main__":
    main()
