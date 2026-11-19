"""Run the UI preview checks (tools/ui-preview) as part of every firmware build.

PlatformIO post-script: the checks run just before the firmware is linked, so
only when something was compiled again, and a failed check stops the build
(and the upload). They need sh and g++ on the PC; without them (e.g. Windows
without Git Bash) they are skipped with a warning.

Skip them once: SKIP_CHECKS=1 pio run
"""

import os
import shutil
import subprocess
from pathlib import Path

Import("env")


def run_checks(source, target, env):
    if os.environ.get("SKIP_CHECKS") == "1":
        print("Checks: skipped (SKIP_CHECKS=1)")
        return 0
    missing = [tool for tool in ("sh", "g++") if shutil.which(tool) is None]
    if missing:
        print("Checks: skipped, {} not found".format(" and ".join(missing)))
        return 0

    script = Path(env.subst("$PROJECT_DIR")) / "tools" / "ui-preview" / "build.sh"
    result = subprocess.run(["sh", str(script), "--checks-only"], capture_output=True, text=True)
    lines = (result.stdout + result.stderr).splitlines()

    if result.returncode == 0:
        groups = sum(1 for line in lines if line.endswith("checks passed"))
        print("Checks: all passed ({} groups)".format(groups))
        return 0

    # A failed check, or the preview didn't compile: show why, stop the build
    failed = [line for line in lines if line.startswith("Check FAILED")]
    print("Checks FAILED, the firmware is not built:")
    for line in failed or lines:
        print("  " + line)
    print("Run tools/ui-preview/build.sh for the details, or SKIP_CHECKS=1 pio run to build anyway.")
    return 1


if not env.IsIntegrationDump():
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", run_checks)
