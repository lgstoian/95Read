@echo off
set TCC=C:\tc\tcc.exe
set TLINK=C:\tc\tlink.exe
set LIB=C:\tc\lib

set INC=-IC:\95read\include
set SRC=src\main.c src\fileio.c src\display.c src\ui.c src\progress.c
set OBJ=main.obj fileio.obj display.obj ui.obj progress.obj

echo Compiling…
%TCC% %INC% -c %SRC%
if errorlevel 1 goto err

echo Linking…
%TLINK% %LIB%\c0s.obj %OBJ% %LIB%\cs.lib,95read.exe,,%LIB%\emu.lib;
if errorlevel 1 goto err

echo Build complete: 95read.EXE
goto end

:err
echo Build failed.
:end
