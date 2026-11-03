#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds the standalone development dex jar of the QindaLink mirror server.
#
#   phone/mirror_server/build-dev-jar.sh [output.jar]
#
# Default output: <repo>/build/mirror/qindalink-mirror-dev.jar. Run it with:
#   adb -s <serial> push <jar> /data/local/tmp/qindalink-mirror-dev.jar
#   adb -s <serial> shell CLASSPATH=/data/local/tmp/qindalink-mirror-dev.jar \
#       app_process / org.qindaqt.link.mirror.Server probe
#
# AGENT-NOTE: the release build compiles the same source directory into the QindaLink APK
# (phone-engine lane). This script exists for development and for the desktop's --server-jar
# path; it uses only the Portage JDK and Android SDK and installs nothing.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "${here}/../.." && pwd)"
out="${1:-${repo}/build/mirror/qindalink-mirror-dev.jar}"

java_home="${JAVA_HOME:-/opt/openjdk-bin-21}"
sdk="${ANDROID_SDK_ROOT:-/opt/android-sdk}"
platform="${QINDALINK_ANDROID_PLATFORM:-android-36}"
build_tools="${QINDALINK_BUILD_TOOLS:-36.0.0}"
android_jar="${sdk}/platforms/${platform}/android.jar"
d8="${sdk}/build-tools/${build_tools}/d8"

for need in "${java_home}/bin/javac" "${android_jar}" "${d8}"; do
    if [[ ! -e "${need}" ]]; then
        echo "build-dev-jar: missing ${need}" >&2
        exit 1
    fi
done

work="$(dirname "${out}")/mirror-server-classes"
rm -rf "${work}"
mkdir -p "${work}/classes" "$(dirname "${out}")"

mapfile -t sources < <(find "${here}/src/main/java" -name '*.java' | sort)
"${java_home}/bin/javac" --release 11 -Xlint:-options -encoding UTF-8 \
    -cp "${android_jar}" -d "${work}/classes" "${sources[@]}"

mapfile -t classes < <(find "${work}/classes" -name '*.class' | sort)
"${d8}" --release --min-api 33 --lib "${android_jar}" --output "${work}" "${classes[@]}"

# d8 writes classes.dex into the output directory; app_process accepts a jar holding it.
rm -f "${out}"
(cd "${work}" && "${java_home}/bin/jar" cf "${out}" classes.dex)
echo "${out}"
