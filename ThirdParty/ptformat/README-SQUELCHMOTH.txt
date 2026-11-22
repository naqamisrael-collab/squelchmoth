libptformat - a library to read ProTools sessions
Copyright (C) 2015-2019 Damien Zammit, Robin Gareus. GNU Lesser General Public License, version 2.1 or later (COPYING.LESSER).
Source: https://github.com/zamaudio/ptformat (commit af15ee9, 2026-06-11), unmodified.

Squelchmoth does not link this library. It is built into a separate small program, sqptread (tools/sqptread/main.cpp),
which Squelchmoth runs to list a session's contents. You may replace sqptread with one built from a modified copy of
the library: build it with scripts/build_ptread.sh, or any C++ compiler:
    c++ -O2 -std=c++11 -I ThirdParty/ptformat ThirdParty/ptformat/ptformat.cc tools/sqptread/main.cpp -o sqptread
