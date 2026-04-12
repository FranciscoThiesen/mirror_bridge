# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- GitHub Codespaces support (`.devcontainer/devcontainer.json`) for instant browser-based development
- `std::optional<T>` type conversion for Python, Lua, and JavaScript
- Async/await support: `std::future<T>` → Python awaitable, JavaScript Promise
- GitHub issue and PR templates for better contribution experience
- Feature matrix table in README showing per-language feature support

### Changed
- Improved devcontainer configuration with proper C++2c reflection flags

## [0.2.0] - 2025-12-01

### Added
- P3394 annotation support for field-level binding control (`[[=exclude{}]]`, `[[=readonly{}]]`)
- Zero-copy buffer protocol for NumPy integration (10M times faster for large data)
- `.pyi` stub generation for Python IDE autocomplete
- Multi-language Mandelbrot demo showcasing Python, Lua, and JavaScript bindings
- `mirror_bridge_doctor` diagnostic tool
- Examples 06-10: callbacks, static/constexpr, free functions, enums, smart pointers
- Migration guide from pybind11
- Comprehensive architecture documentation

### Changed
- Replaced method name mangling with pybind11-style overload dispatch
- Improved method parameter forwarding for move-only and reference types

### Fixed
- GCC compatibility for P3394 annotations
- V8 SetNativeDataProperty API compatibility
- Fold expression empty pack handling

## [0.1.0] - 2025-11-12

### Added
- Initial release with C++26 reflection-based binding generation
- Python bindings via Python C API
- Lua bindings via Lua C API
- JavaScript bindings via Node.js N-API
- Auto-discovery of classes for zero-boilerplate binding
- Container support: `std::vector`, `std::array`
- Smart pointer support: `std::unique_ptr`, `std::shared_ptr`
- Enum support with automatic integer conversion
- Nested object support with dict/table/object conversion
- Method overloading with automatic dispatch
- Static method and constexpr member binding
- Free function binding
- Precompiled header support for 3-6x faster builds
- CLI tools: `mirror_bridge`, `mirror_bridge_auto`, `mirror_bridge_pch_build`
- Docker environment with Bloomberg clang-p2996
- Comprehensive test suite for all languages
- Production-quality examples (image processing, n-body simulation)

### Performance
- 3-5x faster runtime than pybind11
- Zero runtime overhead through compile-time binding generation
- Batch processing API for avoiding Python/C++ boundary overhead

[Unreleased]: https://github.com/FranciscoThiesen/mirror_bridge/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/FranciscoThiesen/mirror_bridge/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/FranciscoThiesen/mirror_bridge/releases/tag/v0.1.0

