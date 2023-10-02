@echo off
if "%1%" == "clean" (
	echo Cleaning builds
	rmdir /s /q "build" && echo removed build
	rmdir /s /q "libraries/glfw-3.3.8/" && echo removed glfw
) else if "%1%" == "test" (
	echo Testing
) else (
	REM Unpack and CMake glfw
	echo Unpacking glfw...
	tar -xf ./libraries/compressed/glfw-3.3.8.zip -C ./libraries/
	REM CMake RAGE
	cmake -B ./build -S ./
	cmake --build ./build
)
