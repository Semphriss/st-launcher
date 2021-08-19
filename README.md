SuperTux launcher
=================

This is a launcher for SuperTux. It allows you to install and manage multiple
versions of SuperTux on the same device, including official releases, nightlies,
forks and more.

Note that the launcher is optional and will always be; if you prefer to install
SuperTux the traditional way, it will always be possible. This launcher is just
a wrapper around the task to make it as easy, quick and accessible as possible.

Overview
--------

This is a stub; most of its features are roughly implemented, and are probably
prone to bugs and other issues. Please use with care **and at your own risk!**

To compile, you must install Curl (with system libraries or
[vcpkg](https://github.com/microsoft/vcpkg)), then run the following commands
from the repository root:

```
mkdir build
cd build
cmake ..
cmake --build .
```

Run the launcher with `./stlauncher` (or `stlauncher.exe` on Windows).

Features
--------

- Automatically detect and list current installations of SuperTux in common
  places, and allow manually adding missing ones;
- Install any release of SuperTux easily;
- Assign each installer version a seperate userdata folder, to avoid clashing;
- Catch game crashes and allow the user to open the log files for review and/or
  to manually send relevant portions to the developers or, at their option, to
  automatically send the logs to the team.

Todo
----

- Make the server provide nightlies in addition to releases;
- Allow users to download versions from forks (consider security issues);
- Allow downloading the source code and compiling from source, if possible;
- Allow dividing versions per category.
- Provide control over CLI arguments (e. g. rendering engine, demo recording...)

License
-------

The SuperTux Launcher is distributed under the terms of the [GNU General Public
License version 3 or later](./LICENSE).

Note that the launcher uses [Harbor](https://github.com/semphriss/harbor), which
is licensed LGPLv3+.
