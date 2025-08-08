# Repository Guidelines

## Project Structure & Module Organization
- `src/`: C++20 sources. Core modules live under `src/k2eg/` with subfolders for `controller`, `service` (epics, pubsub, storage, metric, log, scheduler, configuration, data), and `common`.
- `test/`: GoogleTest suite organized by domain (e.g., `test/controller`, `test/epics`). Test entry point is `test/test.cpp`.
- `doc/`: Reference docs (message format, storage) and images in `doc/image/`.
- `configuration/`: CMake helpers (e.g., `coverage.cmake`).
- `tools/`: Build utilities and patches (e.g., `epics-patch.sh`).
- `docker/`, `Dockerfile.ubuntu`: Containerized builds. Binaries are produced under `build/local/bin/`.

## Build, Test, and Development Commands
- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- Build: `cmake --build build -j`
- Run app: `build/local/bin/k2eg --help`
- Run tests: `ctest --test-dir build --output-on-failure` or `build/local/bin/k2eg-test`
- Sanitizers: `-DENABLE_ASAN=ON` or `-DENABLE_TSAN=ON`
- Coverage: `-DENABLE_COVERAGE=ON` (requires `llvm-cov` or `lcov`/`genhtml`)

## Coding Style & Naming Conventions
- Formatting: `.clang-format` (LLVM-based), 4-space indent, no tabs. Run `clang-format -i` on changed files before committing.
- Naming: Classes use PascalCase (e.g., `EpicsChannel`); member variables and files follow existing patterns (`pv_name`, `EpicsChannel.cpp`). Keep directories lowercase.
- Language: Prefer modern C++ (C++20), avoid raw pointers where possible, and keep headers under `src/` include paths.

## Testing Guidelines
- Framework: GoogleTest with a global environment initializer in `test/test.cpp` (starts/stops EPICS providers).
- Organization: Place tests near their domain (e.g., `test/epics/epics_data_serialization.cpp`). Use clear test names and cover error paths.
- Test uses google test framework

## Commit & Pull Request Guidelines
- Commits: Use imperative mood. Conventional prefixes encouraged: `feat:`, `fix:`, `refactor:`, `test:`, `docs:`. Example: `feat: add MsgPack compact serialization`.
- PRs: Provide a concise description, link issues, list config impacts (Kafka topics, EPICS providers), and include test updates. Ensure CI passes.

## Security & Configuration Tips
- Configuration via CLI flags and `EPICS_k2eg_*` environment variables. Do not commit secrets or real broker endpoints.
- Prefer Docker/devcontainer for reproducible builds; document any local dependencies changed.
