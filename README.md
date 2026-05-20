
[![Travis Build Status](https://travis-ci.org/cortesi/moddwatch.svg?branch=master)](https://travis-ci.org/cortesi/moddwatch)

ModdWatch is a library for building tools that watch files and directories for
modifications.

File watching library for Go backed by [libfswatch](https://github.com/emcrisostomo/fswatch) via CGo.
Uses kqueue on macOS and inotify on Linux. Works with Docker volume mounts.
Fork of [cortesi/moddwatch](https://github.com/cortesi/moddwatch), used by [modd](https://github.com/M2G/modd).