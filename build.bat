@echo off
setlocal

if not exist clang.exe (
  echo clang.exe not found in %cd%
  exit /b 1
)

clang.exe -std=c11 -O2 -msse2 -x c main.c -luser32 -lgdi32 -lole32 -lwindowscodecs -o voxelsurf.exe
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Build succeeded: voxelsurf.exe
