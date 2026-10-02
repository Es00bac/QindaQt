#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile a fork eis plugin source tree with an existing same-commit fork
build's exact flags, into a separate output directory. The reference build is
only read (flags, generated headers, libqindaqt-kwin); nothing is written there.
Usage: build_eis_plugin.py REFERENCE_BUILD REFERENCE_SOURCE EIS_SOURCE_DIR OUT_DIR"""
import concurrent.futures, pathlib, re, shlex, subprocess, sys
ref_build, ref_source, eis_src, out = map(pathlib.Path, sys.argv[1:5])
ninja = (ref_build / "build.ninja").read_text()
def statement(target):
    match = re.search(r"^build " + re.escape(target) + r":(.*?)\n(?=\S|\Z)", ninja, re.S | re.M)
    if not match: raise SystemExit(f"missing reference statement {target}")
    body = match.group(1)
    variables = dict(re.findall(r"^  (\w+) = (.*)$", body, re.M))
    return body, variables
_, ref = statement("src/plugins/eis/CMakeFiles/eis.dir/eisbackend.cpp.o")
old_eis = str(ref_source / "src/plugins/eis")
old_autogen = str(ref_build / "src/plugins/eis/eis_autogen/include")
out.mkdir(parents=True, exist_ok=True)
autogen = out / "autogen"; autogen.mkdir(exist_ok=True)
includes = ref["INCLUDES"].replace(old_autogen, str(autogen)).replace(old_eis, str(eis_src))
defines, flags = ref["DEFINES"], ref["FLAGS"].replace("-fdiagnostics-color=always", "")
moc = "/usr/lib64/qt6/libexec/moc"
moc_includes = re.sub(r"-isystem\s+", "-I", includes)
def run(command):
    result = subprocess.run(command, shell=True, cwd=ref_build, capture_output=True, text=True)
    if result.returncode: raise SystemExit(f"FAILED: {command[:200]}\n{result.stdout}{result.stderr}")
mocs = []
for header in sorted(eis_src.glob("*.h")):
    if "Q_OBJECT" in header.read_text():
        target = autogen / f"moc_{header.stem}.cpp"
        run(f"{moc} {defines} {moc_includes} {shlex.quote(str(header))} -o {target}")
        mocs.append(target)
run(f"{moc} {defines} {moc_includes} {eis_src/'main.cpp'} -o {autogen/'main.moc'}")
cmake = (eis_src / "CMakeLists.txt").read_text()
sources = [eis_src / name for name in re.findall(r"^\s+(\w+\.cpp)$", cmake, re.M)]
sources += [ref_build / "src/plugins/eis/libeis_logging.cpp", ref_build / "src/plugins/eis/inputcapture_logging.cpp"] + mocs
objects = []
def compile_one(source):
    obj = out / (source.name + ".o")
    run(f"/usr/bin/c++ {defines} {includes} {flags} -fPIC -c {source} -o {obj}")
    return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_one, sources))
body, link = statement("bin/qindaqt-kwin/plugins/eis.so")
plugin = out / "plugins/qindaqt-kwin/plugins/eis.so"; plugin.parent.mkdir(parents=True, exist_ok=True)
run(f"/usr/bin/c++ -fPIC {link.get('LANGUAGE_COMPILE_FLAGS','')} {link.get('LINK_FLAGS','')} -shared -o {plugin} "
    + " ".join(map(str, objects)) + f" {link.get('LINK_PATH','')} {link['LINK_LIBRARIES']}")
print(f"BUILT {plugin} from {len(sources)} sources")
