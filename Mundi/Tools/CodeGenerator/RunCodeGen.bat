@echo off
REM Visual Studio Pre-Build Event용 코드 생성 스크립트
REM 프로젝트 전용 Python만 사용

setlocal

REM 프로젝트 루트 Python 경로
set "EMBEDDED_PYTHON=%~dp0..\..\ThirdParty\Python\python.exe"

REM 1. 프로젝트 내장 Python 체크
if exist "%EMBEDDED_PYTHON%" (
    "%EMBEDDED_PYTHON%" "%~dp0generate.py" %*
    exit /b %ERRORLEVEL%
)

REM 2. Python 없음 - 자동 다운로드 및 설치
echo [CodeGen] Project Python not found. Attempting automatic setup...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0EnsurePython.ps1"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to setup Python automatically.
    echo Please check network connection and try again.
    exit /b 1
)

REM 3. 다운로드된 Python으로 실행
if exist "%EMBEDDED_PYTHON%" (
    "%EMBEDDED_PYTHON%" "%~dp0generate.py" %*
    exit /b %ERRORLEVEL%
)

echo [ERROR] Python setup succeeded but executable not found!
exit /b 1
