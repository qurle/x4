# PlatformIO pre-script: injects the short git hash as -DX4_GIT_HASH="abc1234" (with "-dirty" when
# the tree has uncommitted changes). Used by the boot banner and /info.
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)


def git(*args):
    try:
        return subprocess.check_output(["git", *args], cwd=env["PROJECT_DIR"], stderr=subprocess.DEVNULL).decode().strip()  # noqa: F821
    except Exception:
        return ""


rev = git("rev-parse", "--short=7", "HEAD") or "nogit"
if git("status", "--porcelain", "--untracked-files=no"):
    rev += "-dirty"
env.Append(CPPDEFINES=[("X4_GIT_HASH", '\\"%s\\"' % rev)])  # noqa: F821
