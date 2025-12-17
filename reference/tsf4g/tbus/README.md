# TSF4G - tbus (reference)

This directory contains a lightly organized copy of the TSF4G `tbus` shared-memory ring-buffer code,
based on the snippet you provided.

## Layout

- `include/tbus.h`: public API + data structures
- `src/tbus.c`: implementation
- `include/tlibc_error_code.h`: minimal compatibility header (TSF4G originally depends on TLibC)

## Notes

- This is placed under `reference/` as third-party/reference code. It is **not** wired into the main
  `nano-stream` build by default.
- If you want to build it standalone on Linux, see `CMakeLists.txt` in this folder.


