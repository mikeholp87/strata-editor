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
cpack --config build/CPackConfig.cmake -G DEB -B dist
cpack --config build/CPackConfig.cmake -G TGZ
sudo apt install ./dist/strata-editor_0.1.1_amd64.deb
```

DEB declares Qt runtime requirements by hand (each as `libqt6Xt64 | libqt6X`, so it installs on both Ubuntu 24.04 and Debian); the TGZ also needs system Qt and is not an AppImage. AppImage, RPM/Flatpak and signed installers are planned. Tags matching the CMake project version (for example, `v0.1.0`) build, test, install-check and publish a `.deb` plus SHA256 checksums through `.github/workflows/release.yml`. Pull requests and manual dispatches only build candidates. Release packages use Ubuntu 24.04 amd64 so their runtime dependencies match that baseline.

The signed [central APT repository](https://mikeholp87.github.io/apt/) indexes the GitHub Release assets. After a release, run its `publish.yml` workflow to refresh the index; the nightly schedule is a backstop. The [README](../README.md#install-with-apt) has the installation commands.

Windows build/test and ZIP packaging are exercised in GitHub Actions with MSVC 2022 and Qt 6.8.3. For a local build, use MSVC 2022, CMake, and a matching Qt 6 installation. Set CMAKE_PREFIX_PATH to Qt, configure/build, then run windeployqt against strata-editor.exe. CI prepares a ZIP layout with Qt plugins; include all exact Qt license notices before distributing. Installer, signing, native printing and ConPTY have not been verified locally.

Headless tests use QT_QPA_PLATFORM=offscreen. Xvfb is another option. `strata-editor --no-session` bypasses restore. `--smoke-test` launches for 500 ms without restoring sessions. `--screenshot path.png` captures the visible window after 800 ms. Smoke/screenshot runs bypass session restore and do not save a session on automatic exit.
