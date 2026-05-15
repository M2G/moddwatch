
[![Travis Build Status](https://travis-ci.org/cortesi/moddwatch.svg?branch=master)](https://travis-ci.org/cortesi/moddwatch)

ModdWatch is a library for building tools that watch files and directories for
modifications.

Update: Using fswatch via CGO to monitor files.
The main benefit isn't so much the raw speed as it is the reliability, especially on macOS, where notify had known race conditions
(see: a comment in the original code explicitly states this).