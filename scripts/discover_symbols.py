#!/usr/bin/env python3
"""
C++ header parser for auto-discovering classes and free functions.

This script scans C++ header files and outputs discovered symbols in a
pipe-delimited format: qualified_name|simple_name|header_path

Usage:
    discover_symbols.py --classes <header_file> <rel_path>
    discover_symbols.py --functions <header_file> <rel_path> <target_namespaces>

Output format:
    qualified_name|simple_name|header_path
    e.g., "mathlib::Point|Point|math_lib.hpp"
"""

import argparse
import re
import sys
from dataclasses import dataclass
from typing import Iterator


@dataclass
class ParserState:
    """Tracks parsing state while scanning a C++ file.

    Both namespace and class scopes record the brace depth at which they were
    opened so we can pop them correctly when that depth drops, regardless of
    other scopes opened between them.
    """
    # (name, opened_at_depth) tuples.
    namespace_stack: list[tuple[str, int]]
    class_stack: list[tuple[str, int]]
    brace_depth: int
    in_class_or_struct: bool
    class_brace_depth: int
    skip_next: bool
    pending_namespace: str | None
    # Name of a class whose declaration we've seen but whose opening brace
    # we haven't yet — e.g. `class Foo\n    : public Bar {`. When we see the
    # opening brace we push it onto class_stack.
    pending_class: str | None

    def __init__(self):
        self.namespace_stack = []
        self.class_stack = []
        self.brace_depth = 0
        self.in_class_or_struct = False
        self.class_brace_depth = 0
        self.skip_next = False
        self.pending_namespace = None
        self.pending_class = None

    @property
    def current_namespace(self) -> str:
        return "::".join(n for n, _ in self.namespace_stack)

    @property
    def current_class_scope(self) -> str:
        return "::".join(n for n, _ in self.class_stack)


# Regex patterns
NAMESPACE_PATTERN = re.compile(r'^\s*namespace\s+([a-zA-Z_][a-zA-Z0-9_]*)')
CLASS_STRUCT_PATTERN = re.compile(r'^\s*(struct|class)\s+([a-zA-Z_][a-zA-Z0-9_]*)')
FUNCTION_PATTERN = re.compile(
    r'^\s*'
    r'(?:inline\s+)?'
    r'(?:static\s+)?'
    r'(?:constexpr\s+)?'
    r'[a-zA-Z_][a-zA-Z0-9_:*&<>,\s]*\s+'  # return type
    r'([a-zA-Z_][a-zA-Z0-9_]*)\s*'         # function name (captured)
    r'\([^;]*\)\s*'                         # parameters
    r'(?:const\s*)?'                        # optional const
    r'\{'                                   # opening brace
)
SKIP_MARKER = "MIRROR_BRIDGE_SKIP"
PRIVATE_NS_PATTERNS = ["detail", "internal", "impl"]


def count_braces(line: str) -> tuple[int, int]:
    """Count opening and closing braces in a line, ignoring strings and comments."""
    opens = 0
    closes = 0
    in_string = False
    in_char = False
    escape_next = False

    i = 0
    while i < len(line):
        c = line[i]

        if escape_next:
            escape_next = False
            i += 1
            continue

        if c == '\\' and (in_string or in_char):
            escape_next = True
            i += 1
            continue

        # Check for line comment
        if not in_string and not in_char and c == '/' and i + 1 < len(line) and line[i + 1] == '/':
            break  # Rest of line is comment

        if c == '"' and not in_char:
            in_string = not in_string
        elif c == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if c == '{':
                opens += 1
            elif c == '}':
                closes += 1

        i += 1

    return opens, closes


def is_forward_declaration(line: str) -> bool:
    """Check if a class/struct line is a forward declaration."""
    return line.rstrip().endswith(';')


def is_class_definition_start(line: str) -> bool:
    """Check if line starts a class/struct definition (has { or : for inheritance)."""
    # Remove string literals to avoid false positives
    cleaned = re.sub(r'"[^"]*"', '', line)
    cleaned = re.sub(r"'[^']*'", '', cleaned)
    return '{' in cleaned or ':' in cleaned


def is_private_namespace(ns: str) -> bool:
    """Check if namespace matches private patterns (detail, internal, impl, _prefix)."""
    if not ns:
        return False

    parts = ns.split("::")
    for part in parts:
        if part.startswith('_'):
            return True
        for pattern in PRIVATE_NS_PATTERNS:
            if pattern in part.lower():
                return True
    return False


def discover_classes(file_path: str, rel_path: str) -> Iterator[tuple[str, str, str]]:
    """
    Discover class/struct definitions in a header file.

    Yields tuples of (qualified_name, simple_name, header_path).

    The qualified_name includes BOTH namespace and enclosing-class scope, so a
    nested class is reported as `ns::Outer::Inner` and can be used as a template
    argument in generated binding code without needing `using` declarations
    inside the outer class.
    """
    state = ParserState()
    # Stash pending namespace name awaiting its opening brace.
    pending_ns_name: str | None = None

    with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            # Check for skip marker
            if SKIP_MARKER in line:
                state.skip_next = True
                continue

            # Handle namespace declarations. If `{` is on the same line, the
            # push happens below when we process the brace. Either way we
            # remember the name in pending_ns_name.
            ns_match = NAMESPACE_PATTERN.match(line)
            if ns_match:
                pending_ns_name = ns_match.group(1)

            # Check for class/struct definition
            class_match = CLASS_STRUCT_PATTERN.match(line)
            if class_match and not state.skip_next:
                if not is_forward_declaration(line) and is_class_definition_start(line):
                    class_name = class_match.group(2)
                    ns = state.current_namespace
                    scope_prefix = state.current_class_scope
                    parts = [p for p in (ns, scope_prefix, class_name) if p]
                    qualified = "::".join(parts)
                    yield (qualified, class_name, rel_path)
                    state.pending_class = class_name

            # Reset skip flag
            if SKIP_MARKER not in line:
                state.skip_next = False

            # Process each brace in order, updating depth and scope stacks.
            # A '{' that follows a class/namespace declaration opens that scope;
            # any other '{' is an ordinary compound statement / initializer.
            opens, closes = count_braces(line)
            for _ in range(opens):
                state.brace_depth += 1
                if state.pending_class is not None:
                    state.class_stack.append((state.pending_class, state.brace_depth))
                    state.pending_class = None
                elif pending_ns_name is not None:
                    state.namespace_stack.append((pending_ns_name, state.brace_depth))
                    pending_ns_name = None
            for _ in range(closes):
                state.brace_depth -= 1
                while state.class_stack and state.class_stack[-1][1] > state.brace_depth:
                    state.class_stack.pop()
                while state.namespace_stack and state.namespace_stack[-1][1] > state.brace_depth:
                    state.namespace_stack.pop()


def discover_functions(
    file_path: str,
    rel_path: str,
    target_namespaces: set[str]
) -> Iterator[tuple[str, str, str]]:
    """
    Discover free function definitions in specified namespaces.

    Only discovers functions that:
    - Are in one of the target namespaces (or child namespaces)
    - Are not in a private namespace (detail, internal, impl, _)
    - Are not inside a class/struct
    - Are not templates
    - Have a definition (not just declaration)

    Yields tuples of (qualified_name, simple_name, header_path)
    """
    state = ParserState()
    pending_ns_name: str | None = None

    with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            # Check for skip marker
            if SKIP_MARKER in line:
                state.skip_next = True
                # Don't 'continue' - we need to count braces

            # Handle namespace declarations (push happens when brace opens).
            ns_match = NAMESPACE_PATTERN.match(line)
            if ns_match:
                pending_ns_name = ns_match.group(1)

            # Track class/struct entry (to skip member functions)
            class_match = CLASS_STRUCT_PATTERN.match(line)
            if class_match:
                if not is_forward_declaration(line) and is_class_definition_start(line):
                    state.in_class_or_struct = True
                    state.class_brace_depth = state.brace_depth

            # Check for function definition
            if not state.skip_next and not state.in_class_or_struct:
                if 'template' not in line:  # Skip templates
                    func_match = FUNCTION_PATTERN.match(line)
                    if func_match:
                        func_name = func_match.group(1)

                        # Skip operators, destructors, and invalid names
                        if not func_name.startswith('operator') and not func_name.startswith('~'):
                            curr_ns = state.current_namespace

                            # Check if in target namespace and not private
                            if curr_ns and not is_private_namespace(curr_ns):
                                ns_matches = False
                                for target in target_namespaces:
                                    if curr_ns == target or curr_ns.startswith(target + "::"):
                                        ns_matches = True
                                        break

                                if ns_matches:
                                    qualified = f"{curr_ns}::{func_name}"
                                    yield (qualified, func_name, rel_path)

            # Reset skip flag
            if SKIP_MARKER not in line:
                state.skip_next = False

            # Process braces one at a time so namespace pushes bind to the
            # correct opening brace.
            opens, closes = count_braces(line)
            for _ in range(opens):
                state.brace_depth += 1
                if pending_ns_name is not None:
                    state.namespace_stack.append((pending_ns_name, state.brace_depth))
                    pending_ns_name = None
            for _ in range(closes):
                state.brace_depth -= 1
                while state.namespace_stack and state.namespace_stack[-1][1] > state.brace_depth:
                    state.namespace_stack.pop()

            # Exit class/struct when braces close below where it opened.
            if state.in_class_or_struct and state.brace_depth <= state.class_brace_depth:
                state.in_class_or_struct = False


def main():
    parser = argparse.ArgumentParser(
        description='Discover C++ classes and functions in header files'
    )
    parser.add_argument('--classes', action='store_true',
                        help='Discover class/struct definitions')
    parser.add_argument('--functions', action='store_true',
                        help='Discover free function definitions')
    parser.add_argument('header_file', help='Path to header file')
    parser.add_argument('rel_path', help='Relative path for output')
    parser.add_argument('target_namespaces', nargs='?', default='',
                        help='Pipe-separated target namespaces (for --functions)')

    args = parser.parse_args()

    if args.classes:
        for qualified, simple, path in discover_classes(args.header_file, args.rel_path):
            print(f"{qualified}|{simple}|{path}")

    elif args.functions:
        if not args.target_namespaces:
            sys.exit(0)  # No namespaces to search

        namespaces = set(args.target_namespaces.split('|'))
        for qualified, simple, path in discover_functions(args.header_file, args.rel_path, namespaces):
            print(f"{qualified}|{simple}|{path}")

    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
