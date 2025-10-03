@echo off
setlocal EnableExtensions

REM ---- MinGW gcc build of a DLL ----
where gcc >NUL 2>&1 || (
  echo [ERROR] gcc (MinGW-w64) not found in PATH.
  exit /b 1
)

REM Sources (edit to match your tree)
set SRC=powerLimit.c motorController.c serial.c torqueEncoder.c testHelpers.c ^
 IO_RTC_sil.c

REM Compile + link; -lwinpthread is often needed for clock_gettime on MinGW
set CFLAGS=-O2 -I. -DTESTING -D_POSIX_C_SOURCE=199309L
set LDFLAGS=-shared -lwinpthread

gcc %CFLAGS% %LDFLAGS% -o pl_sil.dll %SRC%
if errorlevel 1 exit /b 1

REM Make DLL visible to Python/your tests
set PATH=%CD%;%PATH%

where pytest >NUL 2>&1 && (
  echo Running pytest...
  pytest -q
)

echo Built pl_sil.dll
exit /b 0
