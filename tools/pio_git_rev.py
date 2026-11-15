# PlatformIO pre-script: stamps the build with where it came from.
#
#   X4_GIT_HASH = short git hash ("abc1234", "-dirty" when the tree has uncommitted changes)
#   X4_FW_PR    = the firmware version: the last merged pull request that changed the firmware
#                 (core/, camera/ or shared/), e.g. "#37" (owner, 2026-11-16). One number for the
#                 whole firmware: a Core and a camera built from the same checkout show the same.
#                   "#37"       built exactly from what #37 left (docs-only PRs since don't count)
#                   "#37+"      firmware changed by commits after #37 (a branch, or local work)
#                   "#37+dirty" uncommitted firmware changes in the checkout
#                   "#?"        no merge history (a ZIP download, a shallow clone)
#                 Merges named "Merge pull request #N" (GitHub's merge button) and squash merges
#                 titled "... (#N)" both count.
# Used by the boot banner, /info, registration, the dashboard, the Diagnostics screen and the
# photo metadata (shared/include/x4/version.h: X4_FW_VERSION = X4_FW_PR).
import re
import subprocess

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

FW_PATHS = ["core", "camera", "shared"]
PR_SUBJECT = [re.compile(r"^Merge pull request #(\d+)"), re.compile(r"\(#(\d+)\)\s*$")]


def git(*args, cwd=None):
    try:
        return subprocess.check_output(["git", *args], cwd=cwd or env["PROJECT_DIR"],  # noqa: F821
                                       stderr=subprocess.DEVNULL).decode().strip()
    except Exception:
        return ""


def same(top, a, b):
    """True if the firmware folders are identical in commits a and b."""
    return subprocess.call(["git", "diff", "--quiet", a, b, "--", *FW_PATHS], cwd=top,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0


def pr_label():
    top = git("rev-parse", "--show-toplevel")
    if not top:
        return "#?"
    log = git("log", "-n", "500", "--format=%H%x09%P%x09%s", "HEAD", cwd=top)
    for line in log.splitlines():
        parts = line.split("\t", 2)
        if len(parts) < 3 or not parts[1]:
            continue
        commit, parents, subject = parts
        m = next((p.search(subject) for p in PR_SUBJECT if p.search(subject)), None)
        if not m:
            continue
        if same(top, parents.split()[0], commit):
            continue  # this PR didn't touch the firmware
        label = "#" + m.group(1)
        if not same(top, commit, "HEAD"):
            label += "+"
        if git("status", "--porcelain", "--untracked-files=no", "--", *FW_PATHS, cwd=top):
            label += "dirty" if label.endswith("+") else "+dirty"
        return label
    return "#?"


rev = git("rev-parse", "--short=7", "HEAD") or "nogit"
if git("status", "--porcelain", "--untracked-files=no"):
    rev += "-dirty"
pr = pr_label()
print(f"x4 firmware version {pr} (git {rev})")
env.Append(CPPDEFINES=[("X4_GIT_HASH", '\\"%s\\"' % rev), ("X4_FW_PR", '\\"%s\\"' % pr)])  # noqa: F821
