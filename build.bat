@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ========================================
echo  编译 Chatbot v2.0
echo ========================================
echo.

REM ---- 找 g++ ----
where g++ >nul 2>nul
if not errorlevel 1 (
    for /f "delims=" %%i in ('where g++') do (
        set "GXX=%%i"
        goto :found
    )
)

echo [信息] PATH 里没有 g++，扫描常见位置...
set "CANDIDATES=C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin;C:\Program Files (x86)\Embarcadero\Dev-Cpp\MinGW64\bin;C:\Program Files\Embarcadero\Dev-Cpp\TDM-GCC-64\bin;C:\Dev-Cpp\MinGW64\bin;C:\Dev-Cpp\bin;D:\Dev-Cpp\MinGW64\bin;D:\Dev-Cpp\bin;C:\MinGW\bin;D:\MinGW\bin;C:\msys64\mingw64\bin;C:\msys64\ucrt64\bin;C:\Program Files\CodeBlocks\MinGW\bin"

for %%d in ("%CANDIDATES:;=" "%") do (
    if exist "%%~d\g++.exe" (
        set "GXX=%%~d\g++.exe"
        goto :found
    )
)

echo [错误] 找不到 g++.exe
echo        请把 MinGW 加入 PATH，或手动输入 g++.exe 所在目录。
set /p "USERDIR=请输入路径: "
if "%USERDIR%"=="" (
    pause
    exit /b 1
)
if exist "%USERDIR%\g++.exe" (
    set "GXX=%USERDIR%\g++.exe"
    goto :found
)
echo [错误] 无效路径。
pause
exit /b 1

:found
echo [信息] 使用: !GXX!
echo.

REM 短路径避免空格问题
for %%f in ("!GXX!") do set "GXXDIR=%%~dpf"
if "!GXXDIR:~-1!"=="\" set "GXXDIR=!GXXDIR:~0,-1!"
for %%I in ("!GXXDIR!") do set "SHORTDIR=%%~sI"
set "PATH=!SHORTDIR!;!GXXDIR!;%PATH%"

if not exist "release" mkdir release

REM ---- 生成图标资源 ----
set "RES_ARG="
if exist "app.rc" (
    if exist "app.ico" (
        echo [0/3] 生成图标资源...
        windres app.rc -O coff -o app.res
        if not errorlevel 1 (
            set "RES_ARG=app.res"
            echo      完成。
        ) else (
            echo      [警告] 图标生成失败，跳过。
        )
        echo.
    )
)

REM ---- 编译 ----
echo [1/3] 编译 functions.exe ...
"!GXX!" -std=c++17 -O2 -finput-charset=UTF-8 -fexec-charset=UTF-8 -I src src\functions.cpp !RES_ARG! -o release\functions.exe
if errorlevel 1 (
    echo [错误] 失败。
    pause
    exit /b 1
)
echo      完成。
echo.

echo [2/3] 编译 dict.exe ...
"!GXX!" -std=c++17 -O2 -finput-charset=UTF-8 -fexec-charset=UTF-8 -I src src\dict.cpp !RES_ARG! -o release\dict.exe
if errorlevel 1 (
    echo [错误] 失败。
    pause
    exit /b 1
)
echo      完成。
echo.

echo [3/3] 组装运行包...
if not exist "release\config" mkdir release\config
copy /y data\dict.dat release\ >nul 2>nul
copy /y data\data.dat release\ >nul 2>nul
xcopy /y /e /i data\config release\config >nul 2>nul
if exist "README.md" copy /y README.md release\ >nul 2>nul
echo      完成。
echo.

echo ========================================
echo  编译完成！产物在 release\ 目录
echo ========================================
pause