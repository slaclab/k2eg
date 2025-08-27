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
- Includes: In project `.cpp` files, always include project headers using the `k2eg`-scoped include path with angle brackets, e.g. `#include <k2eg/service/pubsub/ISubscriber.h>`. Avoid relative includes like `#include "../.."`.

### API Documentation (Doxygen)
- Use concise [Doxygen](https://www.doxygen.nl/manual/docblocks.html) blocks for public methods and types.
- Format:
  ```
  /**
   * @brief <short one-line summary>
   * @param <name> <what it represents; units/range if relevant>
   * @return <what is returned; success semantics and error cases>
   */
  ```
- Keep brief lines short and specific; expand only when needed.
- Prefer imperative voice; avoid restating names in descriptions.
- Document ownership/lifetime and thread-safety when non-obvious.

## Testing Guidelines
- Framework: GoogleTest with a global environment initializer in `test/test.cpp` (starts/stops EPICS providers).
- Organization: Place tests near their domain (e.g., `test/epics/epics_data_serialization.cpp`). Use clear test names and cover error paths.
- Test uses google test framework

## Commit & Pull Request Guidelines
- Commits must follow Conventional Commits: `type(scope): subject` in imperative mood. Common types: `feat`, `fix`, `refactor`, `test`, `docs`, `chore`, `build`, `ci`, `revert`. Use optional `!` for breaking changes and add `BREAKING CHANGE:` in the body when applicable.
- Summarize what changed in a short subject line, then include a concise body when helpful.
- Include a "Changes by file" bullet list in the body describing what was done per file, one line per file. Example:
  - `src/k2eg/service/epics/EpicsChannel.cpp: handle disconnect edge-case`
  - `test/epics/epics_data_serialization.cpp: add failing-path test`
- Reference related issues in the footer using `Refs: #123` or `Fixes: #123` as appropriate.
- PRs: Provide a concise description, link issues, list config impacts (Kafka topics, EPICS providers), and include test updates. Ensure CI passes.

### Commit Template
- Template: `.gitmessage` at repo root provides a ready-to-use skeleton.
- Enable locally: `git config commit.template .gitmessage`
- Example:
  - `feat(epics): improve disconnect handling`
  - Body: context and rationale (wrapped at ~72 chars)
  - Changes by file:
    - `src/k2eg/service/epics/EpicsChannel.cpp: guard null state`
    - `test/epics/epics_disconnect_test.cpp: add regression case`
  - Footer: `Fixes: #123`


## LLM Commit Message Rules (Short)

- Conventional Commits: `type(scope): subject` (imperative, ≤72 chars).
- Subject, blank line, then concise bullets (what/why).
- Never embed "\n" in one `-m`. Use multiple `-m` or `-F` (file/heredoc).
- Add `BREAKING CHANGE:` line when needed.
- Verify: `git log -1 --pretty=%B`; fix via `git commit --amend`.
- After amend on shared branches: `git push --force-with-lease`.

Template
```
<type>(<scope>): <subject>

- <bullet 1>
- <bullet 2>

Refs: #<issue>

BREAKING CHANGE: <explanation>
```

Examples
```bash
git commit -m "feat(notification): add rule validation" \
           -m "- validate targets per engine" \
           -m "- expose metadata endpoints"

git commit -F - <<'MSG'
feat(api): expose notification metadata

- GET /v1/logbooks/notification/types
- GET /v1/logbooks/notification/engines
MSG
```

## Security & Configuration Tips
- Configuration via CLI flags and `EPICS_k2eg_*` environment variables. Do not commit secrets or real broker endpoints.
- Prefer Docker/devcontainer for reproducible builds; document any local dependencies changed.
