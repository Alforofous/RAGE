@echo off
if "%1%" == "clean" (
	echo Cleaning builds
	rmdir /s /q "build" && echo removed build
	rmdir /s /q "libraries/glfw-3.3.8/" && echo removed glfw
	rmdir /s /q "libraries/glad/" && echo removed glad
	rmdir /s /q "libraries/glm/" && echo removed glm
	rmdir /s /q "libraries/nanogui/" && echo removed nanogui
) else if "%1%" == "test" (
	echo Testing
) else (
	REM Unpack and CMake glfw
	echo Unpacking glfw...
	tar -xf ./libraries/compressed/glfw-3.3.8.zip -C ./libraries/
	tar -xf ./libraries/compressed/glad.zip -C ./libraries/
	tar -xf ./libraries/compressed/glm.zip -C ./libraries/
	tar -xf ./libraries/compressed/nanogui.zip -C ./libraries/
	REM CMake RAGE
	cmake -B ./build -S ./
	cmake --build ./build
)
