# Pre-build step: sets the firmware version.
#
# Release builds pass the version in the CLAWD_VERSION environment variable
# (taken from the git tag). Local builds use "git describe", so a dev build
# shows which commit it was built from.
import os
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO)

version = os.environ.get("CLAWD_VERSION", "").strip()
if not version:
    try:
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env["PROJECT_DIR"],  # noqa: F821
            stderr=subprocess.DEVNULL,
        ).decode().strip()
    except Exception:
        version = ""
if not version:
    version = "dev"
if version.startswith("v"):
    version = version[1:]

env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])  # noqa: F821
print("version: " + version)
