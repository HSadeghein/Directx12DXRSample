# Nvidia RTX Sample
This project is the sample of Nvidia Realtime-Raytracing using the Microsoft DXR API in the DirectX12.
This sample has not done yet and I am still working on it.
The source code of **Fallback Layer** has been added if you want to see, but if you just want to work with the **Fallback Layer** you just need a build of it.


### Further development
* Utilizing multiple GPU
* Supporting other operating systems by using Vulkan
### Prerequisites
* Visual Studio 2017 version 15.8.4 or higher.
* Windows 10 October 2018 (17763) SDK.
* Add following directories to  C/C++ -> General -> Additional Include       Directories 
    * **$(ProjectDir)FallbackLayer\Include**
    * **$(IntDir)**
    * **$(ProjectDir)Packages**
* Add following directories to Linker -> General -> Additional Linker Directories
    * **$(ProjectDir)FallbackLayer**
    * **$(ProjectDir)Packages\WinPixEventRuntime.1.0.180612001\bin**
* Add following directories to Linker -> Input -> Additional Dependencies
    * **d3d12.lib**
    * **dxgi.lib**
    * **pathcch.lib**
    * **D3DCompiler.lib**
    * **WinPixEventRuntime.lib**
    * **FallbackLayer.lib**
* Add following code to Build Events -> Post-Build Event -> Command Line
    *  **`xcopy "$(ProjectDir)color.hlsl" "$(TargetDir)" /Y`**
    *  **`xcopy "$(ProjectDir)FallbackLayer\dxrfallbackcompiler.dll" "$(TargetDir)" /Y`**
    *  **`xcopy "$(ProjectDir)FallbackLayer\WinPixEventRuntime.dll" "$(TargetDir)" /Y`**
    *  **`xcopy "$(ProjectDir)Additional DLLs\d3dcompiler_47.dll" "$(TargetDir)" /Y`**
    *  **`xcopy "$(ProjectDir)Additional DLLs\dxil.dll" "$(TargetDir)" /Y`**
    *  **`xcopy "$(ProjectDir)Additional DLLs\dxcompiler.dll" "$(TargetDir)" /Y `**
### Showcase
![](sample.gif)
