## What is it?

This is a [Cycles render](https://www.cycles-renderer.org/) integration into [Softimage](https://en.wikipedia.org/wiki/Autodesk_Softimage). This version is a second generation of the addon. It was completely rewritten from scratch. Works with Softimage version 2015 SP2. This is a latest version of the Softimage, so, no reasons to use other ones. Landing page of the addon is [here](https://ssoftadd.github.io/syclesSecondGenPage.html).

## How to install

Download compiled and packed version of the addon from the release page, and install the addon as usual. The addon requires ```nvcuda.dll``` library, installed into system. If you have an NVIDIA GPU, this library is already installed. In other cases (if the GPU is integrated or has a different architecture), simply download this file from [here](https://github.com/Tugcga/Sycles/releases/download/binaries.2.1/nvcuda.dll) and copy it to the ```Sycles\Application\Plugins\bin\nt-x86-64``` folder.

## How to compile

Repository contains solution for Visual Studio 2019. This is minimal version for using Cycles render pre-built libraries. For compile addon from scratch some third-party libraries are required.

* Download ```dst.zip``` and ```dll.zip``` archives from [here](https://github.com/Tugcga/Sycles/releases/tag/binaries.2.1)
* Unpack ```dst.zip``` and place the folder ```dst``` near ```src``` folder in the repository directory
* Open ```src\SyclesPlugin.sln``` solution in Visual Studio and build it
* Copy all ```.dll``` files from the archive ```dll.zip``` to the directory ```Application\Plugins\bin\nt-x86-64``` and place it near the compiled file ```SyclesPlugin.dll```

## Documentaion

If there are some issues or questions about using the addon - create a thread on [Issues](https://github.com/Tugcga/Sycles/issues) page. It's possible to use old documentation for the first generation addon (Sycles 1.12) from [here](https://github.com/ssoftadd/SSoftAdd.github.io/releases/download/1.0.13/sycles_howTo_15_03_2022.pdf). Also official Cycles render [documentation](https://docs.blender.org/manual/en/dev/render/cycles/) can be helpfull.

## Version numbering

All versions of the addon has the number ```2.x.y```. The first number is always ```2```. It means the second generation of the addon. The number ```x``` starts from ```0``` and should be increased every time the Cycles render engine libraries are recompiled and updated. If ```x``` is changed, then ```y``` starts from ```0```. It should be increased every time when addon code is updated for a given version of the Cycles libraries.