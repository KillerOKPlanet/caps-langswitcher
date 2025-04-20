# caps-langswitcher
![demo](https://github.com/user-attachments/assets/b7afe0c6-b7cc-489a-b2c1-a3db867ba30d)

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
    g++ -mwindows -o langswitcher.exe langsw.cpp resource.res -ld2d1 -ldwrite
    ```
5. Place exe into startup location:
![image](https://github.com/user-attachments/assets/4f607527-9613-440f-86f8-afbc357f3f29)

```
shell:startup
```

