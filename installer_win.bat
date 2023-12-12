@echo off

if "%1%" == "clean" (
	echo Cleaning builds
	rmdir /s /q "build" && echo removed build
	rmdir /s /q "libraries/glfw/" && echo removed glfw
	rmdir /s /q "libraries/glad/" && echo removed glad
	rmdir /s /q "libraries/glm/" && echo removed glm
	rmdir /s /q "libraries/imgui/" && echo removed imgui
	del /Q /F "RAGE.exe" && del /Q /F "RAGE.pdb" && echo removed RAGE
) else if "%1%" == "test" (
	echo Testing
) else (
	REM Unpack and CMake glfw
	echo Unpacking glfw...
	tar -xf ./libraries/compressed/glfw.zip -C ./libraries/
	tar -xf ./libraries/compressed/glad.zip -C ./libraries/
	tar -xf ./libraries/compressed/glm.zip -C ./libraries/
	tar -xf ./libraries/compressed/imgui.zip -C ./libraries/

	REM CMake RAGE
	cmake -B ./build -S ./
	cmake --build ./build
)
