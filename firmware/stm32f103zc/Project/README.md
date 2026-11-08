# Project files directory

This directory is reserved for IDE-specific project files:

- Keil MDK-ARM `.uvprojx` and `.uvoptx`
- IAR `.ewp` / `.eww`
- Makefile or CMake configuration files

The source code under `Core/` is intentionally independent of IDE build system so it can be dropped into a chosen toolchain.
