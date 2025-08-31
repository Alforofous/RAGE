param($command)

# Function to check if a C++ compiler is available
function Test-CppCompiler {
    $compilers = @("clang++", "g++", "cl")
    
    foreach ($compiler in $compilers) {
        try {
            $version = & $compiler --version 2>$null
            if ($version) {
                Write-Host "Found compiler: $compiler" -ForegroundColor Green
                return $true
            }
        }
        catch {
            # Compiler not found
        }
    }
    
    return $false
}

# Function to install Clang compiler
function Install-Clang {
    Write-Host "No C++ compiler found. Installing Clang..." -ForegroundColor Yellow
    
    # Download Clang installer
    $clangUrl = "https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/LLVM-17.0.6-win64.exe"
    $installerPath = "$env:TEMP\clang-installer.exe"
    
    try {
        Write-Host "Downloading Clang..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $clangUrl -OutFile $installerPath
        
        Write-Host "Installing Clang..." -ForegroundColor Yellow
        Start-Process -FilePath $installerPath -ArgumentList "/S" -Wait
        
        # Refresh environment variables
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
        
        Write-Host "Clang installed successfully!" -ForegroundColor Green
    }
    catch {
        Write-Host "Failed to install Clang automatically. Please install manually from: https://releases.llvm.org/download.html" -ForegroundColor Red
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
    finally {
        if (Test-Path $installerPath) {
            Remove-Item $installerPath -Force
        }
    }
}

# Function to check if CMake is installed
function Test-CMake {
    try {
        $cmakeVersion = cmake --version 2>$null
        if ($cmakeVersion) {
            return $true
        }
    }
    catch {
        # CMake not found in PATH
    }
    
    # Check common installation paths
    $commonPaths = @(
        "C:\Program Files\CMake",
        "C:\Program Files (x86)\CMake",
        "$env:USERPROFILE\AppData\Local\Programs\CMake"
    )
    
    foreach ($path in $commonPaths) {
        if (Test-Path "$path\bin\cmake.exe") {
            # Add to PATH for current session
            $env:PATH = "$path\bin;$env:PATH"
            return $true
        }
    }
    
    return $false
}

# Function to install CMake
function Install-CMake {
    Write-Host "CMake not found. Installing..." -ForegroundColor Yellow
    
    # Download CMake installer
    $cmakeUrl = "https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-windows-x86_64.msi"
    $installerPath = "$env:TEMP\cmake-installer.msi"
    
    try {
        Write-Host "Downloading CMake..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $cmakeUrl -OutFile $installerPath
        
        Write-Host "Installing CMake..." -ForegroundColor Yellow
        Start-Process -FilePath "msiexec.exe" -ArgumentList "/i", $installerPath, "/quiet", "/norestart" -Wait
        
        # Refresh environment variables
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
        
        # Add CMake to current session PATH
        $cmakePaths = @(
            "C:\Program Files\CMake\bin",
            "C:\Program Files (x86)\CMake\bin",
            "$env:USERPROFILE\AppData\Local\Programs\CMake\bin"
        )
        
        foreach ($path in $cmakePaths) {
            if (Test-Path $path) {
                $env:PATH = "$path;$env:PATH"
                break
            }
        }
        
        Write-Host "CMake installed successfully!" -ForegroundColor Green
    }
    catch {
        Write-Host "Failed to install CMake automatically. Please install manually from: https://cmake.org/download/" -ForegroundColor Red
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
    finally {
        if (Test-Path $installerPath) {
            Remove-Item $installerPath -Force
        }
    }
}

# Function to check if Vulkan SDK is installed
function Test-VulkanSDK {
    # Always check for the best possible Vulkan SDK path, even if environment variable is set
    $commonPaths = @(
        "C:\VulkanSDK",
        "C:\Program Files\VulkanSDK",
        "C:\Program Files (x86)\VulkanSDK"
    )
    
    foreach ($path in $commonPaths) {
        if (Test-Path $path) {
            Write-Host "Found Vulkan SDK at: $path" -ForegroundColor Green
            
            # Check for versioned subdirectory
            $versionedPaths = Get-ChildItem -Path $path -Directory -Name "1.*"
            
            # Sort by version number properly
            $sortedVersions = $versionedPaths | ForEach-Object {
                try {
                    [PSCustomObject]@{
                        Name = $_
                        Version = [Version]$_
                    }
                } catch {
                    [PSCustomObject]@{
                        Name = $_
                        Version = [Version]"0.0.0.0"
                    }
                }
            } | Sort-Object Version -Descending
            
            if ($sortedVersions.Count -gt 0) {
                $latestVersion = $sortedVersions[0].Name
                $fullPath = Join-Path $path $latestVersion
                Write-Host "Found Vulkan SDK version: $latestVersion at: $fullPath" -ForegroundColor Green
                
                # Verify the installation is complete
                if (Test-Path "$fullPath/Include/vulkan/vulkan.h") {
                    Write-Host "Vulkan SDK installation appears complete" -ForegroundColor Green
                    $env:VULKAN_SDK = $fullPath
                    return $true
                } else {
                    Write-Host "Vulkan SDK installation appears incomplete - vulkan.h not found" -ForegroundColor Yellow
                    Write-Host "Would you like to reinstall? (y/n)" -ForegroundColor Yellow
                    $response = Read-Host
                    if ($response -eq "y" -or $response -eq "Y") {
                        Write-Host "Removing incomplete Vulkan SDK..." -ForegroundColor Yellow
                        Remove-Item -Recurse -Force "$fullPath" -ErrorAction SilentlyContinue
                        $env:VULKAN_SDK = $null
                        return $false
                    }
                }
            } else {
                # No versioned subdirectory, use the base path
                if (Test-Path "$path/Include/vulkan/vulkan.h") {
                    Write-Host "Vulkan SDK installation appears complete" -ForegroundColor Green
                    $env:VULKAN_SDK = $path
                    return $true
                } else {
                    Write-Host "Vulkan SDK installation appears incomplete - vulkan.h not found" -ForegroundColor Yellow
                }
            }
        }
    }
    
    # Fallback to environment variable if no paths found
    $vulkanPath = $env:VULKAN_SDK
    if ($vulkanPath -and (Test-Path $vulkanPath)) {
        if (Test-Path "$vulkanPath/Include/vulkan/vulkan.h") {
            Write-Host "Found Vulkan SDK at: $vulkanPath" -ForegroundColor Green
            return $true
        } else {
            Write-Host "Vulkan SDK at $vulkanPath appears incomplete - vulkan.h not found" -ForegroundColor Yellow
        }
    }
    
    return $false
}

# Function to install Vulkan SDK
function Install-VulkanSDK {
    Write-Host "Vulkan SDK not found. Installing..." -ForegroundColor Yellow
    
    # Download Vulkan SDK installer
    $vulkanUrl = "https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe"
    $installerPath = "$env:TEMP\vulkan-sdk.exe"
    
    try {
        Write-Host "Downloading Vulkan SDK..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $vulkanUrl -OutFile $installerPath
        
        Write-Host "Installing Vulkan SDK..." -ForegroundColor Yellow
        Start-Process -FilePath $installerPath -ArgumentList "--accept-licenses", "--default-answer", "--quiet" -Wait
        
        # Refresh environment variables
        $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
        
        Write-Host "Vulkan SDK installed successfully!" -ForegroundColor Green
    }
    catch {
        Write-Host "Failed to install Vulkan SDK automatically. Please install manually from: https://vulkan.lunarg.com/sdk/home" -ForegroundColor Red
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
    finally {
        if (Test-Path $installerPath) {
            Remove-Item $installerPath -Force
        }
    }
}

# Function to clean the build directory
function Clean-Build {
	Write-Host "Cleaning builds" -ForegroundColor Red
	$ProgressPreference = 'SilentlyContinue'
	Remove-Item -Recurse -Force "build" -ErrorAction SilentlyContinue; Write-Host "removed build" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/glfw/" -ErrorAction SilentlyContinue; Write-Host "removed glfw" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/glm/" -ErrorAction SilentlyContinue; Write-Host "removed glm" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/imgui/" -ErrorAction SilentlyContinue; Write-Host "removed imgui" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/spirv-reflect/" -ErrorAction SilentlyContinue; Write-Host "removed spirv-reflect" -ForegroundColor Red
	Remove-Item -Recurse -Force "libraries/googletest/" -ErrorAction SilentlyContinue; Write-Host "removed googletest" -ForegroundColor Red
	Remove-Item -Force "RAGE.exe", "RAGE.pdb" -ErrorAction SilentlyContinue; Write-Host "removed RAGE" -ForegroundColor Red
}

# Function to rebuild the project
function Rebuild-Project {
	Clean-Build
        # Force CMake reconfiguration after clean
        if (Test-Path "./build") {
            Remove-Item -Recurse -Force "./build"
        }
	Build-Project
}

# Function to build the project
function Build-Project {
	Write-Host "Building project" -ForegroundColor Green
	if (-not (Test-CppCompiler)) {
		Write-Host "No C++ compiler found. Would you like to install Clang? (y/n)" -ForegroundColor Yellow
		$response = Read-Host
		if ($response -eq "y" -or $response -eq "Y") {
			Install-Clang
		} else {
			Write-Host "C++ compiler is required for building. Exiting." -ForegroundColor Red
			exit 1
		}
		
		# Verify compiler is now available
		if (-not (Test-CppCompiler)) {
			Write-Host "Clang installation failed or PATH not updated. Please restart PowerShell and try again." -ForegroundColor Red
			exit 1
		}
	}
	
	# Check and install CMake if needed
	if (-not (Test-CMake)) {
		Write-Host "CMake not found. Would you like to install it? (y/n)" -ForegroundColor Yellow
		$response = Read-Host
		if ($response -eq "y" -or $response -eq "Y") {
			Install-CMake
		} else {
			Write-Host "CMake is required for building. Exiting." -ForegroundColor Red
			exit 1
		}
		
		# Verify CMake is now available
		if (-not (Test-CMake)) {
			Write-Host "CMake installation failed or PATH not updated. Please restart PowerShell and try again." -ForegroundColor Red
			exit 1
		}
	} else {
		Write-Host "CMake found!" -ForegroundColor Green
	}
	
	# Check and install Vulkan SDK if needed
	if (-not (Test-VulkanSDK)) {
		Write-Host "Vulkan SDK not found. Would you like to install it? (y/n)" -ForegroundColor Yellow
		$response = Read-Host
		if ($response -eq "y" -or $response -eq "Y") {
			Install-VulkanSDK
		} else {
			Write-Host "Skipping Vulkan SDK installation. Build may fail if Vulkan is required." -ForegroundColor Yellow
		}
	} else {
		Write-Host "Vulkan SDK found!" -ForegroundColor Green
	}
	
	# Verify Vulkan SDK is properly accessible
	Write-Host "Verifying Vulkan SDK accessibility..." -ForegroundColor Yellow
	if ($env:VULKAN_SDK) {
		Write-Host "VULKAN_SDK environment variable: $env:VULKAN_SDK" -ForegroundColor Green
	} else {
		Write-Host "VULKAN_SDK environment variable not set!" -ForegroundColor Red
		Write-Host "Please restart PowerShell or manually set VULKAN_SDK environment variable." -ForegroundColor Red
		exit 1
	}
	
	if (!(Test-Path "./libraries/glfw/")) { tar -xf ./libraries/compressed/glfw.zip -C ./libraries/; Write-Host "Unpacked glfw" -ForegroundColor Green }
	if (!(Test-Path "./libraries/glm/")) { tar -xf ./libraries/compressed/glm.zip -C ./libraries/; Write-Host "Unpacked glm" -ForegroundColor Green }
	if (!(Test-Path "./libraries/imgui/")) { tar -xf ./libraries/compressed/imgui.zip -C ./libraries/; Write-Host "Unpacked imgui" -ForegroundColor Green }
	if (!(Test-Path "./libraries/spirv-reflect/")) { tar -xf ./libraries/compressed/spirv-reflect.zip -C ./libraries/; Write-Host "Unpacked spirv-reflect" -ForegroundColor Green }
	if (!(Test-Path "./libraries/googletest/")) { tar -xf ./libraries/compressed/googletest.zip -C ./libraries/; Write-Host "Unpacked googletest" -ForegroundColor Green }

	if (!(Test-Path "./build/")) { 
		# Try different generators in order of preference
		$generators = @("Ninja", "MinGW Makefiles", "Unix Makefiles", "Visual Studio 17 2022")
		$generatorUsed = $false
		
		foreach ($generator in $generators) {
			try {
				Write-Host "Trying generator: $generator" -ForegroundColor Yellow
				
				# Clean build directory if it exists and has wrong generator
				if (Test-Path "./build/CMakeCache.txt") {
					Remove-Item -Recurse -Force "./build"
				}
				
				cmake -B ./build -S ./ -G $generator
				if ($LASTEXITCODE -eq 0) {
					Write-Host "Successfully configured with generator: $generator" -ForegroundColor Green
					$generatorUsed = $true
					break
				}
			}
			catch {
				Write-Host "Generator $generator failed, trying next..." -ForegroundColor Yellow
			}
		}
		
		if (-not $generatorUsed) {
			Write-Host "Failed to configure with any generator. Please install a C++ compiler." -ForegroundColor Red
			exit 1
		}
		
		Write-Host "CMake RAGE" -ForegroundColor Green 
	} else {
		# Build directory exists, try to build but handle cache errors
		Write-Host "Build directory exists, attempting to build..." -ForegroundColor Yellow
	}
	
	# Build the project and check for success
	cmake --build ./build
	if ($LASTEXITCODE -eq 0) {
		Write-Host "Successfully built RAGE" -ForegroundColor Green
	} else {
		Write-Host "Build failed!" -ForegroundColor Red
		
		# Check if it's a cache error
		$buildOutput = cmake --build ./build 2>&1 | Out-String
		if ($buildOutput -like "*could not load cache*" -or $buildOutput -like "*cache*error*") {
			Rebuild-Project
		} else {
			Write-Host "Build failed for other reasons. Check the error messages above." -ForegroundColor Red
			exit 1
		}
	}
}

# Test command - just test and exit
if ($command -eq "test") {
	Write-Host "Testing" -ForegroundColor Green
	exit 0
}

# Clean command - just clean and exit
if ($command -eq "clean") {
	Clean-Build
    exit 0
}

# Rebuild command - clean first, then continue to build
if ($command -eq "re") {
	Clean-Build
}
Build-Project