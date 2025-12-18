#!/usr/bin/env python3
"""Generate .pyi stub files from compiled mirror_bridge modules.

This script inspects compiled Python extension modules (.so) and generates
type stub files (.pyi) for IDE autocompletion and static type checking.

Usage:
    python3 generate_stubs.py <module_path> [--output <output_dir>]

Examples:
    python3 generate_stubs.py build/point2d.cpython-312-darwin.so
    python3 generate_stubs.py build/calculator.so --output stubs/
"""

import argparse
import importlib.util
import inspect
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Set


# C++ to Python type mappings for common types
CPP_TO_PYTHON_TYPES = {
    # Primitive types
    'int': 'int',
    'long': 'int',
    'long long': 'int',
    'unsigned int': 'int',
    'unsigned long': 'int',
    'size_t': 'int',
    'float': 'float',
    'double': 'float',
    'bool': 'bool',
    'char': 'str',

    # String types
    'std::string': 'str',
    'std::basic_string<char>': 'str',
    'string': 'str',
    'const char*': 'str',

    # Container types
    'std::vector': 'list',
    'vector': 'list',
    'std::list': 'list',
    'std::array': 'list',
    'std::set': 'set',
    'std::unordered_set': 'set',
    'std::map': 'dict',
    'std::unordered_map': 'dict',

    # Smart pointers (usually transparent)
    'std::shared_ptr': '',
    'std::unique_ptr': '',
    'shared_ptr': '',
    'unique_ptr': '',

    # Void
    'void': 'None',
}


def load_module(module_path: str) -> Any:
    """Load a Python extension module from path."""
    path = Path(module_path).resolve()
    if not path.exists():
        raise FileNotFoundError(f"Module not found: {path}")

    # Extract module name from filename (e.g., point2d.cpython-312-darwin.so -> point2d)
    module_name = path.stem.split('.')[0]

    # Add parent directory to path for imports
    parent_dir = str(path.parent)
    if parent_dir not in sys.path:
        sys.path.insert(0, parent_dir)

    spec = importlib.util.spec_from_file_location(module_name, str(path))
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load module: {path}")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


def get_docstring_signature(obj: Any) -> Optional[str]:
    """Extract signature from docstring if available.

    Many binding generators put signatures in docstrings like:
        method(arg1: int, arg2: str) -> bool
    """
    doc = getattr(obj, '__doc__', None)
    if not doc:
        return None

    # Look for signature pattern at start of docstring
    lines = doc.strip().split('\n')
    first_line = lines[0] if lines else ''

    # Common patterns: "name(args) -> return" or "name(args)"
    if '(' in first_line and ')' in first_line:
        return first_line.strip()

    return None


def infer_type_from_value(value: Any) -> str:
    """Infer Python type annotation from a value."""
    if value is None:
        return 'None'
    elif isinstance(value, bool):
        return 'bool'
    elif isinstance(value, int):
        return 'int'
    elif isinstance(value, float):
        return 'float'
    elif isinstance(value, str):
        return 'str'
    elif isinstance(value, bytes):
        return 'bytes'
    elif isinstance(value, list):
        if value:
            elem_type = infer_type_from_value(value[0])
            return f'list[{elem_type}]'
        return 'list'
    elif isinstance(value, dict):
        return 'dict'
    elif isinstance(value, set):
        return 'set'
    elif isinstance(value, tuple):
        return 'tuple'
    else:
        return type(value).__name__


def parse_cpp_type(cpp_type: str) -> str:
    """Convert C++ type string to Python type annotation."""
    cpp_type = cpp_type.strip()

    # Handle const and reference qualifiers
    cpp_type = cpp_type.replace('const ', '').replace('&', '').strip()

    # Handle pointer types after stripping const
    if cpp_type == 'char*':
        return 'str'

    # Direct mappings
    if cpp_type in CPP_TO_PYTHON_TYPES:
        return CPP_TO_PYTHON_TYPES[cpp_type]

    # Container templates: vector<T>, map<K, V>, etc.
    if '<' in cpp_type and '>' in cpp_type:
        base = cpp_type[:cpp_type.index('<')]
        inner = cpp_type[cpp_type.index('<')+1:cpp_type.rindex('>')]

        base_clean = base.replace('std::', '').strip()

        if base_clean in ('vector', 'list', 'array', 'deque'):
            inner_py = parse_cpp_type(inner)
            return f'list[{inner_py}]'
        elif base_clean in ('set', 'unordered_set'):
            inner_py = parse_cpp_type(inner)
            return f'set[{inner_py}]'
        elif base_clean in ('map', 'unordered_map'):
            # Split on comma, handling nested templates
            parts = split_template_args(inner)
            if len(parts) >= 2:
                key_py = parse_cpp_type(parts[0])
                val_py = parse_cpp_type(parts[1])
                return f'dict[{key_py}, {val_py}]'
            return 'dict'
        elif base_clean in ('shared_ptr', 'unique_ptr'):
            return parse_cpp_type(inner)
        elif base_clean == 'optional':
            inner_py = parse_cpp_type(inner)
            return f'{inner_py} | None'

    # Strip namespaces for class types
    if '::' in cpp_type:
        cpp_type = cpp_type.split('::')[-1]

    return cpp_type if cpp_type else 'Any'


def split_template_args(args: str) -> List[str]:
    """Split template arguments, respecting nested templates."""
    result = []
    current = ""
    depth = 0

    for char in args:
        if char == '<':
            depth += 1
            current += char
        elif char == '>':
            depth -= 1
            current += char
        elif char == ',' and depth == 0:
            result.append(current.strip())
            current = ""
        else:
            current += char

    if current.strip():
        result.append(current.strip())

    return result


def get_method_signature(method: Any, method_name: str, is_static: bool = False) -> str:
    """Generate signature string for a method."""
    # Try to get signature from docstring first
    doc_sig = get_docstring_signature(method)
    if doc_sig:
        # Parse docstring signature
        # Format: "method(arg1: type1, arg2: type2) -> ReturnType"
        if '->' in doc_sig:
            args_part, return_part = doc_sig.rsplit('->', 1)
            return_type = return_part.strip()
        else:
            args_part = doc_sig
            return_type = 'Any'

        # Extract just the arguments part
        if '(' in args_part and ')' in args_part:
            start = args_part.index('(')
            end = args_part.rindex(')')
            args = args_part[start:end+1]

            # Clean up C++ types in signature
            args = clean_signature_types(args)
            return_type = parse_cpp_type(return_type)

            if is_static:
                return f"def {method_name}{args} -> {return_type}: ..."
            else:
                # Ensure self is first parameter
                if args == '()':
                    args = '(self)'
                elif not args.startswith('(self'):
                    args = '(self, ' + args[1:]
                return f"def {method_name}{args} -> {return_type}: ..."

    # Fall back to generic signature
    if is_static:
        return f"def {method_name}(*args, **kwargs) -> Any: ..."
    else:
        return f"def {method_name}(self, *args, **kwargs) -> Any: ..."


def clean_signature_types(sig: str) -> str:
    """Clean up C++ types in a signature string."""
    # This is a simplified cleanup - full parsing would be more complex
    result = sig

    # Replace common C++ patterns
    for cpp_type, py_type in CPP_TO_PYTHON_TYPES.items():
        if py_type:
            result = result.replace(cpp_type, py_type)

    # Clean up std:: prefix
    result = result.replace('std::', '')

    return result


def generate_class_stub(cls: type, class_name: str, indent: str = "") -> List[str]:
    """Generate stub lines for a class."""
    lines = []
    lines.append(f"{indent}class {class_name}:")

    inner_indent = indent + "    "
    has_members = False

    # Get class docstring
    if cls.__doc__:
        doc_lines = cls.__doc__.strip().split('\n')
        lines.append(f'{inner_indent}"""')
        for doc_line in doc_lines:
            lines.append(f'{inner_indent}{doc_line}')
        lines.append(f'{inner_indent}"""')
        has_members = True

    # Collect all members
    members = []
    static_methods = []
    instance_methods = []
    class_attrs = []

    for name in dir(cls):
        if name.startswith('_') and not name.startswith('__'):
            continue  # Skip private members
        if name in ('__class__', '__dict__', '__module__', '__weakref__',
                    '__doc__', '__delattr__', '__dir__', '__eq__', '__format__',
                    '__ge__', '__getattribute__', '__getstate__', '__gt__',
                    '__hash__', '__init_subclass__', '__le__', '__lt__',
                    '__ne__', '__new__', '__reduce__', '__reduce_ex__',
                    '__repr__', '__setattr__', '__sizeof__', '__str__',
                    '__subclasshook__'):
            continue  # Skip standard dunder methods

        try:
            attr = getattr(cls, name)
        except AttributeError:
            continue

        if isinstance(attr, staticmethod) or (callable(attr) and
            not isinstance(inspect.getattr_static(cls, name), property)):
            # Check if it's truly static by examining the descriptor
            raw_attr = inspect.getattr_static(cls, name)
            if isinstance(raw_attr, staticmethod):
                static_methods.append((name, attr))
            elif callable(attr) and not isinstance(raw_attr, property):
                instance_methods.append((name, attr))
        elif isinstance(inspect.getattr_static(cls, name), property):
            members.append((name, attr))
        elif not callable(attr):
            class_attrs.append((name, attr))

    # Generate __init__ signature
    init_method = getattr(cls, '__init__', None)
    if init_method:
        sig = get_method_signature(init_method, '__init__', is_static=False)
        lines.append(f"{inner_indent}{sig}")
        has_members = True

    # Class attributes (static data members)
    for name, value in sorted(class_attrs):
        type_hint = infer_type_from_value(value)
        lines.append(f"{inner_indent}{name}: {type_hint}")
        has_members = True

    # Properties (data members exposed as properties)
    for name, prop in sorted(members):
        # Try to infer type from getter
        type_hint = 'Any'
        if hasattr(prop, 'fget') and prop.fget:
            doc_sig = get_docstring_signature(prop.fget)
            if doc_sig and '->' in doc_sig:
                return_type = doc_sig.split('->')[-1].strip()
                type_hint = parse_cpp_type(return_type)

        lines.append(f"{inner_indent}@property")
        lines.append(f"{inner_indent}def {name}(self) -> {type_hint}: ...")

        # Check if setter exists
        if hasattr(prop, 'fset') and prop.fset:
            lines.append(f"{inner_indent}@{name}.setter")
            lines.append(f"{inner_indent}def {name}(self, value: {type_hint}) -> None: ...")
        has_members = True

    # Static methods
    for name, method in sorted(static_methods):
        lines.append(f"{inner_indent}@staticmethod")
        sig = get_method_signature(method, name, is_static=True)
        lines.append(f"{inner_indent}{sig}")
        has_members = True

    # Instance methods
    for name, method in sorted(instance_methods):
        sig = get_method_signature(method, name, is_static=False)
        lines.append(f"{inner_indent}{sig}")
        has_members = True

    if not has_members:
        lines.append(f"{inner_indent}...")

    return lines


def generate_function_stub(func: Any, func_name: str, indent: str = "") -> str:
    """Generate stub line for a free function."""
    sig = get_method_signature(func, func_name, is_static=True)
    return f"{indent}{sig}"


def generate_enum_stub(enum_cls: type, enum_name: str, indent: str = "") -> List[str]:
    """Generate stub lines for an enum class."""
    lines = []
    lines.append(f"{indent}class {enum_name}:")

    inner_indent = indent + "    "

    # Get enum values
    for name in dir(enum_cls):
        if not name.startswith('_'):
            try:
                value = getattr(enum_cls, name)
                if not callable(value):
                    lines.append(f"{inner_indent}{name}: int")
            except AttributeError:
                pass

    if len(lines) == 1:  # Only the class line
        lines.append(f"{inner_indent}...")

    return lines


def generate_module_stub(module: Any, module_name: str) -> str:
    """Generate complete .pyi stub content for a module."""
    lines = []

    # Header
    lines.append(f'"""Type stubs for {module_name} extension module.')
    lines.append('')
    lines.append('Generated by mirror_bridge stub generator.')
    lines.append('"""')
    lines.append('')
    lines.append('from typing import Any, List, Dict, Set, Optional, Callable, overload')
    lines.append('')

    # Track what we've processed
    processed: Set[str] = set()

    # Process all module members
    classes = []
    functions = []
    constants = []
    enums = []

    for name in dir(module):
        if name.startswith('_'):
            continue

        try:
            obj = getattr(module, name)
        except AttributeError:
            continue

        if isinstance(obj, type):
            # Check if it looks like an enum
            if hasattr(obj, '__members__') or (hasattr(obj, 'value') and
                not any(callable(getattr(obj, n)) for n in dir(obj) if not n.startswith('_'))):
                enums.append((name, obj))
            else:
                classes.append((name, obj))
        elif callable(obj):
            functions.append((name, obj))
        else:
            constants.append((name, obj))

    # Generate constants
    if constants:
        lines.append('# Module constants')
        for name, value in sorted(constants):
            type_hint = infer_type_from_value(value)
            lines.append(f'{name}: {type_hint}')
        lines.append('')

    # Generate enums
    for name, enum_cls in sorted(enums):
        lines.extend(generate_enum_stub(enum_cls, name))
        lines.append('')

    # Generate classes
    for name, cls in sorted(classes):
        lines.extend(generate_class_stub(cls, name))
        lines.append('')

    # Generate functions
    if functions:
        lines.append('# Module functions')
        for name, func in sorted(functions):
            lines.append(generate_function_stub(func, name))
        lines.append('')

    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description='Generate .pyi stub files from compiled mirror_bridge modules'
    )
    parser.add_argument('module_path', help='Path to the compiled module (.so file)')
    parser.add_argument('--output', '-o', help='Output directory for stub files (default: same as module)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')

    args = parser.parse_args()

    module_path = Path(args.module_path).resolve()

    # Determine output path
    if args.output:
        output_dir = Path(args.output)
        output_dir.mkdir(parents=True, exist_ok=True)
    else:
        output_dir = module_path.parent

    # Extract module name
    module_name = module_path.stem.split('.')[0]

    if args.verbose:
        print(f"Loading module: {module_path}")

    try:
        module = load_module(str(module_path))
    except Exception as e:
        print(f"Error loading module: {e}", file=sys.stderr)
        sys.exit(1)

    if args.verbose:
        print(f"Generating stubs for: {module_name}")

    stub_content = generate_module_stub(module, module_name)

    # Write stub file
    stub_path = output_dir / f"{module_name}.pyi"
    stub_path.write_text(stub_content)

    print(f"Generated: {stub_path}")


if __name__ == '__main__':
    main()
