@echo off
REM Simple packaging helper - collects binaries and output into release folder
set REPO_ROOT=%~dp0\..
set BUILD_DIR=%REPO_ROOT%\build
set RELEASE_DIR=%REPO_ROOT%\release\modyplus_deluxe_v1.0

if exist "%RELEASE_DIR%" rd /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

copy "%BUILD_DIR%\modyplus_deluxe.exe" "%RELEASE_DIR%\" >nul 2>&1
xcopy "%REPO_ROOT%\output" "%RELEASE_DIR%\output" /E /I
copy "%REPO_ROOT%\config\sample_rules.json" "%RELEASE_DIR%\" >nul 2>&1
echo Release prepared at %RELEASE_DIR%