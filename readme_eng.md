# MrpackMaker

Qt 6 GUI tool for creating Modrinth `.mrpack` modpacks.

## Features

- Wizard-based workflow
- Basic pack metadata
- Add mod files by direct URL or local `.jar`
- Search Modrinth and CurseForge in-app
- Calculate `sha1`, `sha512`, and file size
- Select `client`, `server`, and `required / optional / unsupported` support
- Build `overrides`, `client-overrides`, and `server-overrides`
- Export in the background with progress output

## Workflow

1. Fill in pack information:
   - name
   - summary
   - version
   - Minecraft version
   - loader type: Fabric, Forge, NeoForge, or Quilt
   - loader version

2. Add files:
   - paste one or more direct download URLs
   - import local `.jar` files
   - search Modrinth or CurseForge and add matching files

3. Calculate hashes and file sizes:
   - downloaded files are cached in `%TEMP%\Mrpack\Mods\`
   - local files are read directly
   - the tool fills in `sha1`, `sha512`, and size automatically

4. Configure overrides:
   - choose an instance folder
   - select folders or files to include
   - repeat for global, client-only, and server-only overrides

5. Export:
   - review the summary
   - choose an output path
   - start export and keep using the app while it runs

## Modrinth and CurseForge search

- Modrinth search does not need an API key.
- CurseForge search requires an API key.
- After a file is added, it is treated like a normal direct-download item.

## CurseForge API key storage

CurseForge API keys are stored with Windows-only encrypted storage.
The private key is protected with DPAPI and the storage path is recorded in the Windows registry.

## Build

### Requirements

- Qt 6.11.0
- MSVC 2022
- QuaZip installed manually
- OpenSSL available to CMake
- vcpkg with `zlib`

### Configure and build

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE=<vcpkg-path>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Notes

- `.mrpack` export uses standard ZIP Deflate compression.
- The app is Windows-only.
