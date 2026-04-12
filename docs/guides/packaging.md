# Packaging and Distribution

How to distribute mirror_bridge bindings as installable Python packages.

## Quick Start

After generating your bindings, create a pip-installable package:

```bash
# Generate the binding
mirror_bridge generate src/ --module my_lib --lang python

# Package it
cd build/
pip install .
```

## Project Layout

A distributable mirror_bridge project looks like:

```
my_project/
├── src/
│   └── my_class.hpp          # Your C++ headers
├── build/
│   ├── my_lib_binding.cpp     # Auto-generated binding
│   └── my_lib.so              # Compiled shared library
├── pyproject.toml              # Package metadata
├── setup.py                    # Build configuration
└── my_lib/
    ├── __init__.py             # Python package init
    └── my_lib.so -> ../build/my_lib.so
```

## pyproject.toml

```toml
[build-system]
requires = ["setuptools>=64"]
build-backend = "setuptools.build_meta"

[project]
name = "my-lib"
version = "0.1.0"
description = "Python bindings for my C++ library via mirror_bridge"
requires-python = ">=3.7"

[project.optional-dependencies]
dev = ["pytest"]

[tool.setuptools.packages.find]
where = ["."]
```

## setup.py (for native extensions)

```python
from setuptools import setup, Extension
import subprocess
import os

# Pre-build the binding using mirror_bridge
def build_binding():
    """Generate and compile the binding if not already built."""
    if not os.path.exists("build/my_lib.so"):
        subprocess.check_call([
            "mirror_bridge", "generate", "src/",
            "--module", "my_lib",
            "--lang", "python"
        ])

build_binding()

setup(
    name="my-lib",
    version="0.1.0",
    packages=["my_lib"],
    package_data={"my_lib": ["*.so"]},
    # Include the compiled .so in the package
    data_files=[("my_lib", ["build/my_lib.so"])],
)
```

## __init__.py

```python
"""Python bindings for my C++ library."""

import os
import sys

# Add the package directory to the path so the .so can be found
_dir = os.path.dirname(os.path.abspath(__file__))
if _dir not in sys.path:
    sys.path.insert(0, _dir)

from my_lib import *  # Import all bound classes
```

## Building a Wheel

```bash
# Install build tools
pip install build

# Build the wheel (requires mirror_bridge compiler in PATH)
python -m build --wheel

# The wheel is in dist/
ls dist/
# my_lib-0.1.0-cp310-cp310-linux_x86_64.whl
```

## Docker-Based Packaging

Since mirror_bridge requires Bloomberg's clang-p2996, you'll typically build inside the Docker container and package the resulting `.so`:

```bash
# Build inside Docker
docker run --rm -v $(pwd):/workspace ghcr.io/franciscothiesen/mirror_bridge:latest \
    bash -c "cd /workspace && mirror_bridge generate src/ --module my_lib --lang python"

# Now package locally (only needs the pre-built .so)
pip wheel . --no-build-isolation
```

## Multi-Platform Wheels

For distributing across platforms, build a wheel for each target:

```bash
# Linux x86_64
docker run --platform linux/amd64 ...

# Linux ARM64
docker run --platform linux/arm64 ...
```

Each produces a platform-specific wheel that can be uploaded to PyPI.

## Publishing to PyPI

```bash
pip install twine

# Upload to TestPyPI first
twine upload --repository testpypi dist/*

# Upload to PyPI
twine upload dist/*
```

## CMake Integration

For projects using CMake, add the binding as a build target:

```cmake
# In your CMakeLists.txt
find_package(Python3 COMPONENTS Development)

mirror_bridge_python_module(my_lib
    SOURCES src/my_class.hpp
)

# Install the .so alongside your Python package
install(TARGETS my_lib DESTINATION my_lib)
```

## Example: Complete Distributable Project

See [examples/packaging/](../../examples/packaging/) for a complete working example with:
- C++ source header
- pyproject.toml configuration
- setup.py with auto-build
- __init__.py with proper imports
- Tests with pytest

## Tips

- **Pin the compiler version**: Use a specific Docker image tag, not `:latest`
- **Cache the PCH**: Pre-build the precompiled header to speed up CI builds
- **Version your .so**: Include the version in the .so filename for ABI tracking
- **Test the wheel**: Install in a clean virtualenv before publishing
