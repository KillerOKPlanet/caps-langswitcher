# caps-langswitcher

A lightweight keyboard layout switcher that uses **CapsLock** to toggle between **English** and the last used language.  
**Shift+CapsLock** works as regular **CapsLock**.

---

## 🛠 Compile instructions (using mingw64)

1. Install **mingw64** to `C:\`
2. Open **cmd**
3. Set PATH:
    ```cmd
    set PATH=C:\mingw64\bin;%PATH%
    ```
4. Compile:
    ```cmd
    g++ -mwindows -o langswitcher16.exe langsw.cpp resource.res -ld2d1 -ldwrite
    ```


