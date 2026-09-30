@echo off
chcp 65001 > nul
setlocal

:: Always build from the project folder, regardless of the caller's current folder.
set "PROJECT_DIR=%~dp0"
cd /d "%PROJECT_DIR%"

:: Initialize the Visual Studio C++ environment.
set "VS_VCVARS=%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS_VCVARS%" (
    echo ERROR: Visual Studio vcvars64.bat was not found.
    pause
    exit /b 1
)
call "%VS_VCVARS%"
if errorlevel 1 (
    echo ERROR: Could not initialize the Visual Studio C++ environment.
    pause
    exit /b 1
)

:: Use VULKAN_SDK when set; otherwise find the newest SDK under C:\VulkanSDK.
if not defined VULKAN_SDK (
    for /f "delims=" %%D in ('dir /b /ad /o-n "C:\VulkanSDK" 2^>nul') do if not defined VULKAN_SDK set "VULKAN_SDK=C:\VulkanSDK\%%D"
)
if not defined VULKAN_SDK (
    echo ERROR: Vulkan SDK was not found. Set the VULKAN_SDK environment variable.
    pause
    exit /b 1
)
set "VULKAN_INC=%VULKAN_SDK%\Include"
set "VULKAN_LIB=%VULKAN_SDK%\Lib"
if not exist "%VULKAN_INC%\vulkan\vulkan.h" (
    echo ERROR: Vulkan headers were not found under "%VULKAN_SDK%".
    pause
    exit /b 1
)
if not exist "%VULKAN_LIB%\vulkan-1.lib" (
    echo ERROR: vulkan-1.lib was not found under "%VULKAN_SDK%".
    pause
    exit /b 1
)

set "IMGUI_DIR=%PROJECT_DIR%third_party\imgui"
set "GLFW_DIR=%PROJECT_DIR%third_party\glfw"
if not exist "%GLFW_DIR%\lib\glfw3.lib" (
    echo ERROR: GLFW library was not found at "%GLFW_DIR%\lib\glfw3.lib".
    pause
    exit /b 1
)

:: Include directories and source files.
set INCLUDES=/I "%VULKAN_INC%" /I "%GLFW_DIR%\include" /I "%IMGUI_DIR%" /I "%IMGUI_DIR%\backends" /I "%PROJECT_DIR%Core_Engine\Stroke_Engine\include" /I "%PROJECT_DIR%Core_Engine\Graphic_Engine\Gpen\include" /I "%PROJECT_DIR%UI\Windows\Base" /I "%PROJECT_DIR%UI\Windows\Canvas" /I "%PROJECT_DIR%UI\Windows\Tool_Bar\Tool_Bar" /I "%PROJECT_DIR%UI\Windows\Tool_Bar\File" /I "%PROJECT_DIR%UI\Windows\Side_Bar"
set SOURCES="UI\Windows\Base\main.cpp" "UI\Windows\Base\VulkanApp.cpp" "UI\Windows\Tool_Bar\Tool_Bar\MainToolBar.cpp" "UI\Windows\Tool_Bar\File\NewCanvasDialog.cpp" "UI\Windows\Side_Bar\ToolSidebar.cpp" "Core_Engine\Stroke_Engine\src\StrokeManager.cpp" "Core_Engine\Graphic_Engine\Gpen\src\StrokeRenderer.cpp" "%IMGUI_DIR%\imgui.cpp" "%IMGUI_DIR%\imgui_draw.cpp" "%IMGUI_DIR%\imgui_tables.cpp" "%IMGUI_DIR%\imgui_widgets.cpp" "%IMGUI_DIR%\backends\imgui_impl_glfw.cpp" "%IMGUI_DIR%\backends\imgui_impl_vulkan.cpp"

cl /nologo /EHsc /MD /W3 /O2 /std:c++17 /utf-8 %INCLUDES% %SOURCES% /Fe:"%PROJECT_DIR%FastPaint.exe" /link /LIBPATH:"%VULKAN_LIB%" user32.lib gdi32.lib shell32.lib kernel32.lib "%VULKAN_LIB%\vulkan-1.lib" "%GLFW_DIR%\lib\glfw3.lib"
if errorlevel 1 (
    echo.
    echo ===================================
    echo  Build failed. Review the errors above.
    echo ===================================
    pause
    exit /b 1
)

echo.
echo ===================================
echo  Build succeeded. Starting FastPaint.exe
echo ===================================
echo.
"%PROJECT_DIR%FastPaint.exe"
set "APP_EXIT_CODE=%ERRORLEVEL%"
pause
exit /b %APP_EXIT_CODE%
