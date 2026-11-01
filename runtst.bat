@echo off
rem Build and run the 95read test harness.
rem Run from the project root (the directory containing src\, include\).
call buildtst.bat
if not exist tests mkdir tests
set READ95CFG=tests\test.cfg
test.exe
