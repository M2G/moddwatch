# ModdWatch

[![test](https://github.com/M2G/moddwatch/actions/workflows/test.yml/badge.svg)](https://github.com/M2G/moddwatch/actions/workflows/test.yml)

ModdWatch is a library for building tools that watch files and directories for
modifications.

This is a fork of [cortesi/moddwatch](https://github.com/cortesi/moddwatch)
that replaces `github.com/bmatcuk/doublestar` and `github.com/rjeczalik/notify`
with a native C implementation (via CGo), backed by
[libfswatch](https://github.com/emcrisostomo/fswatch).

## Requirements

- `libfswatch` installed on the build machine
- `CGO_ENABLED=1`