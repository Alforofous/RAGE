param($command)

if ($command -eq "clean") {
	Write-Host "Cleaning builds" -ForegroundColor Red
	Remove-Item -Recurse -Force "build" -ErrorAction SilentlyContinue; Write-Host "removed build" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/glfw/" -ErrorAction SilentlyContinue; Write-Host "removed glfw" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/glad/" -ErrorAction SilentlyContinue; Write-Host "removed glad" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/glm/" -ErrorAction SilentlyContinue; Write-Host "removed glm" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/imgui/" -ErrorAction SilentlyContinue; Write-Host "removed imgui" -ForegroundColor Red
	Remove-Item -Force "RAGE.exe", "RAGE.pdb" -ErrorAction SilentlyContinue; Write-Host "removed RAGE" -ForegroundColor Red
} elseif ($command -eq "test") {
	Write-Host "Testing" -ForegroundColor Green
} else {
	if (!(Test-Path "./libraries/glfw/")) { tar -xf ./libraries/compressed/glfw.zip -C ./libraries/; Write-Host "Unpacked glfw" -ForegroundColor Green }
	if (!(Test-Path "./libraries/glad/")) { tar -xf ./libraries/compressed/glad.zip -C ./libraries/; Write-Host "Unpacked glad" -ForegroundColor Green }
	if (!(Test-Path "./libraries/glm/")) { tar -xf ./libraries/compressed/glm.zip -C ./libraries/; Write-Host "Unpacked glm" -ForegroundColor Green }
	if (!(Test-Path "./libraries/imgui/")) { tar -xf ./libraries/compressed/imgui.zip -C ./libraries/; Write-Host "Unpacked imgui" -ForegroundColor Green }

	if (!(Test-Path "./build/")) { cmake -B ./build -S ./; Write-Host "CMake RAGE" -ForegroundColor Green }
	cmake --build ./build; Write-Host "Successfully built RAGE" -ForegroundColor Green
}