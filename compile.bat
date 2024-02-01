cls

if not exist ".\build\" mkdir .\build\

REM DEBUG BUILD

cl -DUNICODE /ZI /EHsc /W3 /D DEBUG=1 /D PUZZLE /Febuild/Win32Debug.exe PlatformWin32.cpp user32.lib gdi32.lib d2d1.lib Ole32.lib D3D11.lib Windowscodecs.lib Dwrite.lib

REM RELEASE BUILD

REM cl -DUNICODE /O2 /fp:fast /Zi /Febuild/Win32Release.exe PlatformWin32.cpp user32.lib gdi32.lib d2d1.lib Ole32.lib D3D11.lib Windowscodecs.lib Dwrite.lib


REM SOFTWARE RASTERIZED BUILD

REM cl /Zi /EHsc /W3 /D DEBUG=1 /Febuild/Win32SoftwareRenderer.exe PlatformWin32SoftwareRenderer.cpp User32.lib Gdi32.lib


REM SOFTWARE RASTERIZED BUILD RELEASE

REM cl /Zi /arch:AVX /O2 /fp:fast /EHsc /D DEBUG=0 /Febuild/Win32SoftwareRendererRelease.exe PlatformWin32SoftwareRenderer.cpp User32.lib Gdi32.lib