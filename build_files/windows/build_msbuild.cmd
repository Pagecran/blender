if "%NOBUILD%"=="1" goto EOF
echo %TIME% > %BUILD_DIR%\buildtime.txt

if exist "%BUILD_DIR%\Blender.slnx" (
	set SOLUTION_FILE=%BUILD_DIR%\Blender.slnx
) else (
	set SOLUTION_FILE=%BUILD_DIR%\Blender.sln
)

msbuild ^
	%SOLUTION_FILE% ^
	/target:build ^
	/property:Configuration=%BUILD_TYPE% ^
	/maxcpucount ^
	/verbosity:minimal ^
	/p:platform=%MSBUILD_PLATFORM% ^
	/flp:Summary;Verbosity=minimal;LogFile=%BUILD_DIR%\Build.log 
	if errorlevel 1 (
		echo Error during build, see %BUILD_DIR%\Build.log for details 
		exit /b 1
	)

msbuild ^
	%BUILD_DIR%\INSTALL.vcxproj ^
	/property:Configuration=%BUILD_TYPE% ^
	/verbosity:minimal ^
	/p:platform=%MSBUILD_PLATFORM% 
	if errorlevel 1 (
		echo Error during install phase
		exit /b 1
	)
echo %TIME% >> %BUILD_DIR%\buildtime.txt
:EOF