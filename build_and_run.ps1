 $VCPKG_TOOLCHAIN = "C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"

Write-Host "=== Step 1: Install dependencies ===" -ForegroundColor Cyan
vcpkg install
if ($LASTEXITCODE -ne 0) { Write-Host "FAILED at vcpkg install" -ForegroundColor Red; exit 1 }

Write-Host "=== Step 2: Clean old build ===" -ForegroundColor Cyan
if (Test-Path build)   { Remove-Item -Recurse -Force build }
if (Test-Path vcpkg_installed) { Remove-Item -Recurse -Force vcpkg_installed }

Write-Host "=== Step 3: Install again (fresh) ===" -ForegroundColor Cyan
vcpkg install
if ($LASTEXITCODE -ne 0) { Write-Host "FAILED at vcpkg install" -ForegroundColor Red; exit 1 }

Write-Host "=== Step 4: Configure CMake ===" -ForegroundColor Cyan
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { Write-Host "FAILED at cmake configure" -ForegroundColor Red; exit 1 }

Write-Host "=== Step 5: Build ===" -ForegroundColor Cyan
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { Write-Host "FAILED at cmake build" -ForegroundColor Red; exit 1 }

Write-Host "=== Step 6: Find exe ===" -ForegroundColor Cyan
 $exe = Get-ChildItem -Recurse -Filter "VulkanPBR.exe" build | Select-Object -First 1
if (-not $exe) { Write-Host "ERROR: VulkanPBR.exe not found after build!" -ForegroundColor Red; exit 1 }

Write-Host "Found exe at: $($exe.FullName)" -ForegroundColor Green

Write-Host "=== Step 7: Run ===" -ForegroundColor Cyan
Set-Location $exe.Directory
& $exe.FullName