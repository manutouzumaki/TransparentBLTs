@echo off

IF NOT EXIST ..\build mkdir ..\build

pushd ..\build
cl -nologo /Zi -wd4005 ..\code\main.cpp User32.lib Gdi32.lib Kernel32.lib
popd
