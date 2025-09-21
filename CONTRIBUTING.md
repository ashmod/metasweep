# Contributing to metasweep

Thank you for your interest in contributing to metasweep! This document provides guidelines and information for contributors.

## Table of Contents

- [Development Setup](#development-setup)
- [Building](#building)
- [Testing](#testing)
- [Code Style](#code-style)
- [Submitting Changes](#submitting-changes)
- [Reporting Issues](#reporting-issues)
- [Code of Conduct](#code-of-conduct)

## Development Setup

### Prerequisites

Before you begin, ensure you have the following installed:

- **C++ Compiler**: GCC 9+ or Clang 10+ (C++20 support required)
- **CMake**: Version 3.20 or later
- **Git**: For version control

### Dependencies

The project depends on several libraries. Installation commands for common platforms:

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y cmake build-essential libexiv2-dev libtag1-dev zlib1g-dev pkg-config
```

#### macOS
```bash
brew install cmake exiv2 taglib zlib pkg-config
```

#### Windows
- Install Visual Studio with C++ support, or MinGW-w64
- Dependencies are handled via vcpkg or similar (see CMake configuration)

### Cloning the Repository

```bash
git clone https://github.com/ashmod/metasweep.git
cd metasweep
```

## Building

metasweep uses CMake as its build system.

### Standard Build

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j$(nproc)

# Install (optional)
cmake --install build --prefix /usr/local
```

### Development Build

For development with debug symbols and additional checks:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Build Options

You can customize the build using these CMake options:

- `ENABLE_BACKEND_EXIV2=ON/OFF`: Enable/disable image metadata support (default: ON)
- `ENABLE_BACKEND_POPPLER=ON/OFF`: Enable/disable PDF metadata support (default: ON)
- `ENABLE_BACKEND_TAGLIB=ON/OFF`: Enable/disable audio metadata support (default: ON)
- `ENABLE_BACKEND_MINIZIP=ON/OFF`: Enable/disable ZIP metadata support (default: ON)
- `BUILD_SHARED_LIBS=OFF/ON`: Build shared libraries instead of static (default: OFF)

Example:
```bash
cmake -S . -B build -DENABLE_BACKEND_POPPLER=OFF -DCMAKE_BUILD_TYPE=Debug
```

## Code Style

### C++ Guidelines

- Use C++20 features where appropriate
- Follow modern C++ best practices (RAII, smart pointers, etc.)
- Prefer `const` correctness
- Use meaningful variable and function names
- Keep functions small and focused on a single responsibility

### Formatting

The repository includes a `.clang-format` that defines our canonical C/C++ style (LLVM-based):

- 2-space indentation, no tabs
- 100-character column limit
- Sorted/regrouped includes
- Brace style: Attach; pointer alignment: left

Please format sources with clang-format before committing:

```bash
# From repo root
clang-format -i $(git ls-files '*.h' '*.hpp' '*.c' '*.cc' '*.cpp' '*.cxx')
```

Optional: add a pre-commit hook to auto-format staged files:

```bash
cat > .git/hooks/pre-commit <<'HOOK'
#!/usr/bin/env bash
set -euo pipefail
files=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\\.(h|hpp|c|cc|cpp|cxx)$' || true)
[ -z "$files" ] && exit 0
clang-format -i $files
git add $files
HOOK
chmod +x .git/hooks/pre-commit
```

### Documentation

- Document public APIs with Doxygen-style comments
- Keep code self-documenting where possible
- Update documentation when changing APIs

## Submitting Changes

### Pull Request Process

1. **Fork** the repository on GitHub
2. **Create a feature branch** from `main`:
   ```bash
   git checkout -b feature/your-feature-name
   ```
3. **Make your changes** following the guidelines above
4. **Test thoroughly** - ensure all tests pass and no regressions
5. **Update documentation** if needed
6. **Commit with clear messages**:
   ```bash
   git commit -m "feat: add new feature description"
   ```
7. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```
8. **Create a Pull Request** on GitHub with:
   - Clear title and description
   - Reference any related issues
   - Screenshots/demo for UI changes

### Commit Message Guidelines

Follow conventional commit format:
- `feat:` for new features
- `fix:` for bug fixes
- `docs:` for documentation changes
- `test:` for test additions/modifications
- `refactor:` for code refactoring
- `chore:` for maintenance tasks

Example: `feat: add support for WebP image metadata`

## Reporting Issues

### Bug Reports

When reporting bugs, please include:

- **Clear title** describing the issue
- **Steps to reproduce** the problem
- **Expected behavior** vs actual behavior
- **Environment details**:
  - OS and version
  - Compiler version
  - metasweep version (or commit hash)
- **Sample files** if applicable (avoid sharing sensitive data)
- **Error messages** or logs

### Feature Requests

For feature requests:

- **Clear description** of the proposed feature
- **Use case** - why would this be useful?
- **Implementation ideas** if you have any
- **Alternative solutions** you've considered

### Security Issues

For security-related issues, please email the maintainers directly rather than creating a public issue.

## Code of Conduct

This project follows a code of conduct to ensure a welcoming environment for all contributors. By participating, you agree to:

- Be respectful and inclusive
- Focus on constructive feedback
- Accept responsibility for mistakes
- Show empathy towards other contributors
- Help create a positive community

## Getting Help

If you need help or have questions:

- Check existing issues and documentation first
- Create a new issue with detailed information
- Join community discussions if available

Thank you for contributing to metasweep!
