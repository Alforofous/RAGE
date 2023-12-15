#!/bin/bash

command=$1

if [ "$command" == "clean" ]; then
	echo -e "\033[31mCleaning builds\033[0m"
	rm -rf "build"; echo -e "\033[31mremoved build\033[0m"
	rm -rf "libraries/glfw/"; echo -e "\033[31mremoved glfw\033[0m"
	rm -rf "libraries/glad/"; echo -e "\033[31mremoved glad\033[0m"
	rm -rf "libraries/glm/"; echo -e "\033[31mremoved glm\033[0m"
	rm -rf "libraries/imgui/"; echo -e "\033[31mremoved imgui\033[0m"
	rm -f "RAGE.exe" "RAGE.pdb"; echo -e "\033[31mremoved RAGE\033[0m"
elif [ "$command" == "test" ]; then
	echo -e "\033[32mTesting\033[0m"
else
	if [ ! -d "./libraries/glfw/" ]; then tar -xf ./libraries/compressed/glfw.zip -C ./libraries/; echo -e "\033[32mUnpacked glfw\033[0m"; fi
	if [ ! -d "./libraries/glad/" ]; then tar -xf ./libraries/compressed/glad.zip -C ./libraries/; echo -e "\033[32mUnpacked glad\033[0m"; fi
	if [ ! -d "./libraries/glm/" ]; then tar -xf ./libraries/compressed/glm.zip -C ./libraries/; echo -e "\033[32mUnpacked glm\033[0m"; fi
	if [ ! -d "./libraries/imgui/" ]; then tar -xf ./libraries/compressed/imgui.zip -C ./libraries/; echo -e "\033[32mUnpacked imgui\033[0m"; fi

	if [ ! -d "./build/" ]; then cmake -B ./build -S ./; echo -e "\033[32mCMake RAGE\033[0m"; fi
	cmake --build ./build; echo -e "\033[32mSuccessfully built RAGE\033[0m"
fi