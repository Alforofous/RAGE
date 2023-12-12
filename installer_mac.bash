#!/bin/bash

if [ "$1" == "clean" ]; then
	echo "Cleaning builds"
	rm -rf "build" && echo "removed build"
	rm -rf "libraries/glfw/" && echo "removed glfw"
	rm -rf "libraries/glad/" && echo "removed glad"
	rm -rf "libraries/glm/" && echo "removed glm"
	rm -rf "libraries/imgui/" && echo "removed imgui"
elif [ "$1" == "test" ]; then
	echo "Testing"
else
	# Unpack and CMake glfw
	echo "Unpacking glfw..."
	tar -xf ./libraries/compressed/glfw.zip -C ./libraries/
	tar -xf ./libraries/compressed/glad.zip -C ./libraries/
	tar -xf ./libraries/compressed/glm.zip -C ./libraries/
	tar -xf ./libraries/compressed/imgui.zip -C ./libraries/

	# CMake RAGE
	./compileRAGE
fi
