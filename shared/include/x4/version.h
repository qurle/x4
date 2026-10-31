// x4 firmware identity. Shared by the Core and the camera firmware.
#pragma once

// Semantic version of the firmware. Bump on every release you flash to more than one board.
#define X4_FW_VERSION "0.1.0"

// Injected by tools/pio_git_rev.py (extra_scripts) as -DX4_GIT_HASH=\"abc1234\".
#ifndef X4_GIT_HASH
#define X4_GIT_HASH "nogit"
#endif

#define X4_BUILD_TIME __DATE__ " " __TIME__

// Wire protocol version (UDP + HTTP). Peers with a different major refuse to talk.
// See docs/PROTOCOL.md "Versioning".
#define X4_PROTOCOL_VERSION 1
