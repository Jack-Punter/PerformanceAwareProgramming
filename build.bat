@echo off
REM Run Steup the toolchain if CL doesn't exist on the path
call where cl >nul 2>&1 || call .\setup_cl.bat

REM -------------------------- Set Helper Variables --------------------------
set me="%~dp0"
cd %me%
set root=%cd%
set build_dir=%root%\build
set src_dir=%root%\src
set include_dir=%root%\include

set CommonCompilerFlags=-MT -nologo -Gm- -Zi -FC -TC -GR- -EHa -W3 -fp:fast -fp:except- -arch:AVX2 -std:c11
REM set CommonCompilerFlags=%CommonCompilerFlags% -diagnostics:caret
set CommonCompilerFlags=%CommonCompilerFlags% -D_CRT_SECURE_NO_WARNINGS

set DebugCompilerFlags=-Od
set ReleaseCompilerFlags=-O2

REM -------------------------- Set Build Options --------------------------
set BuildFlags=%CommonCompilerFlags% %DebugCompilerFlags%

REM -------------------------- Build the project --------------------------
IF NOT EXIST %build_dir% mkdir %build_dir%

REM Clear the build directory 
REM echo y | del %build_dir% > NUL 2> NUL

pushd %build_dir%
  del *.pdb > NUL 2> NUL

  cl %BuildFlags% %src_dir%\main.c -I%src_dir% /Fe8086.exe /link -incremental:no %CommonLinkerFlags%
popd

EXIT /B %ERRORLEVEL%
