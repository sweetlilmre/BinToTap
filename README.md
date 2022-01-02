# BinToTap
Transforms C64 binary header loaders and PRG files into TAP files

This is based off of the excellent work of Ingo Korb's TapeCart project: https://github.com/ikorb/tapecart

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
