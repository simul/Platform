.. Platform documentation master file, created by
   sphinx-quickstart
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Platform
========

Platform is a cross-platform rendering library and API.

About Platform
--------------
The Platform library was developed by Simul for use in trueSKY (simul.co/trueSKY), and has seen use in many indie and triple-A game titles and professional simulations. It provides a cross-platform API for modern rendering, abstracting the backend while maintaining support for up-to-date features. With Platform, you can have multiple API's working in a single process.

Shaders in Platform are compiled either offline or on-the-fly from sfx source files. Similar to the old fx or cgfx effect files, these can contain multiple shaders of different types as well as render state setup.

The intention with Platform is that it should take few lines of code to write real-time graphics applications, but that the final code should be well-optimized for rendering.

At the moment, due to its origins in trueSKY, Platform is quite focused on Compute and has incomplete support for other shader types.

Supported Platforms
-------------------
 * Windows 10
 * Linux
 * PS4
 * Xbox One
 * Others...

Supported Rendering API's
-------------------------
 * Direct3D 11
 * Direct3D 12
 * PS4
 * OpenGL
 * Vulkan

 Required Software
 ---------------
 * CMake
 * Python 3
 * Git
 * GitPython
 * Visual Studio 2017 or later (Windows)
 * Clang 8+ (Linux)


Setup
-----
To run the Python setup script, ensure that the git Python module is installed.

	pip install GitPython

Now run Setup.py to update the submodules and build Platform for Windows x64.

Configuring
-----------
With CMakeGui or a similar tool, you can configure Platform to your requirements. For example, individual API's such as Vulkan or D3D12 can be enabled or disabled.

Compiling
---------
By default for Windows, a Visual Studio solution Platform.sln will be created and built in Platform/build. To build for another platform:

Use the CMake cross-compiling toolchain file in Platform/[PlatformName]/CMake.
Ensure that the x64 version is built first. In particular, other platforms use the Sfx effect compiler which is created at Platform/build/bin/Release/Sfx.exe.


.. toctree::
   :maxdepth: 2
   :caption: Reference:
   
   /API/apiindex.rst
   /Reference/reference.rst


Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

