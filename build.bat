@echo off

call :build bootstrap "src/*.c"

exit /b %errorlevel%

:build

if not exist build\\ mkdir build\\

set options=/nologo /WX /W4
set options=%options% /Fobuild/

set debug_opts=/ZI /Fdbuild/
set debug_opts=%debug_opts% /D_DEBUG

cl %options% %debug_opts% /Febuild/%~1.exe %~2

exit /b %errorlevel%

:end