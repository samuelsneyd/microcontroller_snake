# Snake
This is a snake game for the TTGO T-Display Board.
It uses PlatformIO, the esp-idf, and a simple graphics library.
It has four difficulties: easy, medium, hard, and expert.

To try it out, install Vscode, add the the PlatformIO IDE extension, open the folder and click 
the arrow -> in the status bar (PlatformIO:Upload).

There are 2 environments defined in the platformio.ini file,
an emulator(env:emulator) and a real board (env:tdisplay). 
The default is use the emulator but you can upload to a real board by setting the environment
in the platformio status bar to env:tdisplay.

The emulator should work on Windows, Linux and OSX.

![Snake Demo](static/qemu-system-xtensa_66CHw3GaDy.png?raw=true "Snake Demo")
