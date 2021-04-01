@echo off

set HEADERS=C:/Work/themachinery
set FLAGS=-I %HEADERS% -Wno-microsoft-anon-tag -fms-extensions -Werror
set BIN=C:\Work\themachinery\bin\Debug

%BIN%/hash

@echo on

zig cc -shared -o tm_zig.dll zig-plugin.c %FLAGS%
copy tm_zig.dll %BIN%\plugins