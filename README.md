# Overview
vkDOOM3 adds a Vulkan renderer to DOOM 3 BFG Edition.  It was written as an example of how to use Vulkan for writing something more sizable than simple recipes.  It covers topics such as General Setup, Proper Memory & Resource Allocation, Synchronization, Pipelines, etc.

# Building

## Windows

Prerequisites:

* [Git for Windows](https://github.com/git-for-windows/git/releases)
* [Vulkan SDK](https://vulkan.lunarg.com/) (several versions tested, but best to go with latest)
* A [Vulkan-capable GPU](https://en.wikipedia.org/wiki/Vulkan_(API)#Compatibility) with the appropriate drivers installed

Start `Git Bash` and clone the vkDOOM3 repo:

~~~
git clone https://github.com/DustinHLand/vkDOOM3.git
~~~

### Visual Studio

Install [Visual Studio Community](https://www.visualstudio.com) with Visual C++ component.

Open the Visual Studio solution, `neo\doom3.sln`, select the desired configuration and platform, then
build the solution. (GL=OpenGL, VK=Vulkan)

### MinGW

Not currently supported.

## Linux

Not currently supported.

# Usage

You must own a copy of DOOM 3 BFG.  You can purchase it on Steam (http://store.steampowered.com/app/208200/Doom_3_BFG_Edition/).  From there you can copy the assets over your repo (.gitignore is setup to handle this), or you can pass +set fs_basePath <path_to_steam_assets> on the command line.