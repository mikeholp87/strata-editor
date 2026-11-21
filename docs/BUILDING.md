# Building and installation

Linux dependencies: C++20 compiler, CMake >=3.22, Qt >=6.4 Core/Gui/Widgets/Concurrent/PrintSupport/Test. Debian package: qt6-base-dev. Ninja is optional. No network access is required by CMake; all dependencies must already exist.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$HOME/.local"
```

Ensure ~/.local/bin is on PATH. Desktop entry and SVG icon install under share. Uninstall by removing only the files listed in build/install_manifest.txt. Do not remove configuration or recovery files unless you intend to discard them.

```sh
cpack --config build/CPackConfig.cmake -G DEB
cpack --config build/CPackConfig.cmake -G TGZ
sudo apt install ./strata-editor-0.1.0-Linux.deb
```

DEB uses dpkg-shlibdeps to declare system-library requirements; the TGZ also needs system Qt and is not an AppImage. AppImage, RPM/Flatpak and signed installers are planned. No release is published automatically.

Windows: use MSVC 2022, CMake, and a matching Qt 6 installation. Set CMAKE_PREFIX_PATH to Qt, configure/build, then run windeployqt against strata-editor.exe. CI prepares a ZIP layout with Qt plugins; include all exact Qt license notices before distributing. Installer, signing, native printing and ConPTY have not been verified locally.

Headless tests use QT_QPA_PLATFORM=offscreen. Xvfb is another option. `strata-editor --no-session` bypasses restore. `--smoke-test` launches for 500 ms without restoring sessions. `--screenshot path.png` captures the visible window after 800 ms. Smoke/screenshot runs bypass session restore and do not save a session on automatic exit.
