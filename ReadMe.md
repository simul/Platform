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
-------------------

 * Direct3D 11
 * Direct3D 12
 * PS4
 * OpenGL
 * Vulkan

 Required
 ---------

 * CMake
 * Python 3
 * Git
 * GitPython
 * Visual Studio 2017 or later (Windows)
 * Clang 8+ (Linux)
