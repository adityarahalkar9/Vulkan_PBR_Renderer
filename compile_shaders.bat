@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo Compiling Shaders with glslangValidator
echo ========================================================

if "%VULKAN_SDK%"=="" (
    echo [ERROR] VULKAN_SDK environment variable is not set!
    exit /b 1
)

set GLSLANG="%VULKAN_SDK%\Bin\glslangValidator.exe"

if not exist %GLSLANG% (
    echo [ERROR] glslangValidator.exe not found at %GLSLANG%
    exit /b 1
)

set SHADER_DIR=%~dp0
set ERRORS=0

echo Compiling pbr.vert...
%GLSLANG% -V "%SHADER_DIR%pbr.vert" -o "%SHADER_DIR%pbr.vert.spv"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Failed to compile pbr.vert
    set /a ERRORS+=1
) else (
    echo [OK] Compiled pbr.vert.spv
)

echo Compiling pbr.frag...
%GLSLANG% -V "%SHADER_DIR%pbr.frag" -o "%SHADER_DIR%pbr.frag.spv"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Failed to compile pbr.frag
    set /a ERRORS+=1
) else (
    echo [OK] Compiled pbr.frag.spv
)

echo Compiling shadow.vert...
%GLSLANG% -V "%SHADER_DIR%shadow.vert" -o "%SHADER_DIR%shadow.vert.spv"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Failed to compile shadow.vert
    set /a ERRORS+=1
) else (
    echo [OK] Compiled shadow.vert.spv
)

echo Compiling shadow.frag...
%GLSLANG% -V "%SHADER_DIR%shadow.frag" -o "%SHADER_DIR%shadow.frag.spv"
if %ERRORLEVEL% neq 0 (
    echo [FAIL] Failed to compile shadow.frag
    set /a ERRORS+=1
) else (
    echo [OK] Compiled shadow.frag.spv
)

if %ERRORS% neq 0 (
    echo ========================================================
    echo [ERROR] Shader compilation finished with %ERRORS% error(s).
    echo ========================================================
    exit /b 1
)

echo ========================================================
echo [SUCCESS] All shaders compiled successfully!
echo ========================================================
exit /b 0
