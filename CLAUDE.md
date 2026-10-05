# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Object Explorer is a native Windows GUI tool (C++20, WTL/ATL) for exploring kernel objects: object types, the object manager namespace, objects, handles (system-wide / per type / per process), and zombie processes/threads.

## Build

Visual Studio solution `ObjectExplorer.sln` (toolset v145). Configurations: `Debug`, `Release`, `ReleaseSigned`; platform `x64` (the solution's ARM64 configurations build the x64 projects). There are no linters.

```
git submodule update --init --recursive    # WTLHelper and WinSys are submodules
msbuild ObjectExplorer.sln /p:Configuration=Debug /p:Platform=x64
x64\Debug\ObjExpTests.exe                  # run the tests
x64\Debug\ObjExpTests.exe "[ProcessHelper]"  # tests with a tag; or a test case name
```

Output goes to `x64\<Configuration>\`. Run from a Developer Command Prompt (msbuild must be on PATH).

## Tests

`ObjExpTests` is a Catch2 (v3) console project for code without UI. Catch2 comes from vcpkg (`x64-windows-static`, via the user-wide vcpkg MSBuild integration); the project has its own `main` since vcpkg doesn't link `Catch2Main` automatically.
- It compiles the ObjExp sources under test directly (`ProcessHelper`, `AccessMaskDecoder`, `ObjectManager`, `DriverHelper`, `NtDll`) and references WTLHelper; add a source there when testing more ObjExp code.
- It builds with exceptions (`_HAS_EXCEPTIONS=1`), which ObjExp's `pch.h` allows to override; ObjExp itself builds without them.
- It's built only in the solution's `Debug|x64` and `Release|x64` configurations.
- Tests run against the live system (handle list, object namespace, processes); they use objects the test process creates where possible, and don't need elevation.
- UI classes aren't testable as is; logic worth testing is moved out of them (e.g. `SearchMatcher`, used by the Find view).
- The `[symbols]` tests load the kernel's PDB through DIA: a post-build step copies `msdia140.dll` (VS DIA SDK) and `symsrv.dll` (Windows SDK debuggers) to the output folder, and `_NT_SYMBOL_PATH` must be set. They skip otherwise.

Solution projects:
- `ObjExp` — the application.
- `DiaHelper` — static lib wrapping the DIA SDK (`DiaSession`/`DiaSymbol`) for reading kernel type layouts from PDBs.
- `WTLHelper` (submodule) — the author's shared WTL controls/helpers (virtual list views, `CNativeCustomTabView`, `ToolbarHelper`, theming/dark mode, `SecurityHelper`, etc.). Also vendors WTL itself under `WTLHelper\WTL`.
- `WinSys` (submodule) is **not** built as part of the solution; only its headers are used, notably `WinSys\WinSys\phnt` (native NT API definitions).

Changes to shared helpers in the submodules affect other projects by the same author — prefer changing code in `ObjExp` unless the helper is clearly generic.

## Conventions

- Precompiled header `pch.h` (every `.cpp` includes it first). Note `_HAS_EXCEPTIONS 0` — no C++ exceptions; use return values / `NTSTATUS` / `bool`.
- `wil` is used for RAII handles (`wil::unique_handle`, etc.); ATL `CString` for strings; `std::format` is available.
- Native APIs go through the `NT::` namespace (`NtDll.h`) on top of phnt definitions.
- New source files must be added to both `ObjExp.vcxproj` and `ObjExp.vcxproj.filters`.

## Architecture

- **Main frame / tabs**: `CMainFrame` (`MainFrm.*`) implements `IMainFrame` (`Interfaces.h`) and hosts a `CNativeCustomTabView`. Each tab is an `IView`. Commands are routed from the frame to the active view via `IView::ProcessCommand`.
- **Views**: `ViewFactory` (singleton) creates views by `ViewType` enum and adds them as tabs. Views derive from `CViewBase<T>` (`ViewBase.h`), a CRTP base over `CFrameWindowImpl` that provides toolbar creation, UI-update access, page activation hooks (`OnPageActivated`, `UpdateUI` overridables), and self-deletion in `OnFinalMessage`. To add a view: add a `ViewType` value, a `CViewBase`-derived class, and a case in `ViewFactory::CreateView`.
- **Data layer**: `ObjectManager` enumerates types (`EnumTypes`), handles (`EnumHandles2<T>`), and objects (`EnumObjects<T>`) from `SystemExtendedHandleInformation`; object names/info are obtained by duplicating handles into this process (`DupHandle`) and calling `NtQueryObject`. Type info is cached in static maps keyed by type index and name. `ObjectHelpers` / `ProcessHelper` / `AccessMaskDecoder` provide type-specific formatting and details.
- **Kernel access** (requires elevation), two separate drivers:
  - `DriverHelper` talks to the author's `KObjExp` driver (`\\.\KObjExp`, versioned via `DRIVER_CURRENT_VERSION`) to open objects by address, duplicate handles from other processes, and get object addresses.
  - `DbgDriver` uses Microsoft's `kldbgdrv.sys` (embedded as resource `IDR_DRIVER`, extracted and installed as a service on demand) for reading/writing kernel virtual memory. Opened at startup only when running elevated (`ObjExp.cpp`).
- **Symbols / structures**: `SymbolManager` + `SymbolHandler` (dbghelp) and `DiaHelper` resolve kernel structure layouts; `StructurePage` / `SymbolToTreeView` display a kernel object's structure, with memory read through `DbgDriver`.
- **Properties dialog**: `ObjectPropertiesDlg` is a tabbed dialog; `ObjectHelpers` assembles its pages (`GenericPage`, `HandlesPage`, `StructurePage`) per object type. Security editing uses `SecurityInfo` (ACL UI).
- **Settings**: `AppSettings` (built on WTLHelper's `Settings` macros) persists window placement, font, dark mode, etc.; `CMainFrame` broadcasts dark-mode changes to all views with `WM_UPDATE_DARKMODE`.
