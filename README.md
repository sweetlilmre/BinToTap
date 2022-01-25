# BinToTap
Transforms C64 binary header loaders and PRG files into TAP files

This is based off of the excellent work of Ingo Korb's TapeCart project: https://github.com/ikorb/tapecart

To complile this for windows:
Install:
1. VSCode
2. MSYS2
3. MINGW64 from MSYS2 (read the MSYS2 page)
4. The C/C++ extension for VSCode

Then open VSCode in the cloned folder and compile using GCC (if you have used default paths for MSYS2 then this should probably just work).
Compliling this under various Linux distributions should be fairly easy (with gcc installed "g++ -o BinToTap BinToTap.cpp" is sufficient).


```
BinToTap v1.0
USAGE:

BinToTap args [input file]
  BinToTap -d
    Will generate a TAP file containting the default TAPECART fastloader from Ingo Korb
    The file will be named TapeCartLoader.tap

  BinToTap -h loaderfile
    Will generate a TAP file containting the loader specified by "loaderfile" this must be exactly 171 bytes in size
    The file will be named HeaderLoader.tap

  BinToTap -p prgfile
    Will generate a TAP file containting the PRG specified by "prgfile"
    The file will be named the same as the PRG file, but with a TAP extension
```
