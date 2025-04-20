# caps-langswitcher
A lightweight keyboard layout switcher that uses CapsLock to toggle between English and last used language. Shift+CapsLock works as regular CapsLock.

Use mingw64 to compile (install to C:\)
set PATH=C:\mingw64\bin;%PATH%
g++ -mwindows -o langswitcher16.exe langsw.cpp resource.res -ld2d1 -ldwrite
