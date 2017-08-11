@echo off

for %%f in (*.vert) do (
	%VULKAN_SDK%\bin\glslangValidator.exe -V %%f -o ..\spirv\%%~nf.vspv
)
for %%f in (*.frag) do (
	%VULKAN_SDK%\bin\glslangValidator.exe -V %%f -o ..\spirv\%%~nf.fspv
)