@echo off
setlocal

where cmake >nul 2>&1 || (echo ERROR: CMake is required. & pause & exit /b 1)

pushd "%~dp0" >nul || (echo ERROR: Failed to open repository directory. & pause & exit /b 1)
cmake --fresh --preset x64 || (echo ERROR: CMake configure failed. & popd & pause & exit /b 1)
cmake --build --preset release || (echo ERROR: C++ build failed. & popd & pause & exit /b 1)
popd >nul

echo Build completed
echo Output: %~dp0build\bin\Release

endlocal
pause
