@echo off
rem Build the test harness (tests\test_main.c) with Turbo C 2.01.
rem Run from the project root (the directory containing src\, include\).
set TCC=..\TC\tcc.exe
set TLINK=..\TC\tlink.exe
set LIB=..\TC\lib
set INC=-Iinclude
set TSRC=tests\tmain.c src\config.c src\fileio.c src\progress.c src\display.c
set TOBJ=tmain.obj config.obj fileio.obj progress.obj display.obj

echo Compiling tests...
%TCC% %INC% -c %TSRC%
if errorlevel 1 goto err

echo Linking tests...
%TLINK% %LIB%\c0s.obj %TOBJ%,test.exe,, %LIB%\cs.lib;%LIB%\emu.lib;
if errorlevel 1 goto err

echo Test build complete: test.exe
goto end

:err
echo Test build failed.
:end
