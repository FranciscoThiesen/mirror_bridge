# Security Policy

## Supported Versions

Mirror Bridge is pre-1.0; only the latest release and `main` receive fixes.

## Reporting a Vulnerability

Please **do not open a public issue** for security vulnerabilities.

Instead, use [GitHub's private vulnerability reporting](https://github.com/FranciscoThiesen/mirror_bridge/security/advisories/new)
("Report a vulnerability" under the Security tab), which reaches the
maintainers privately.

You can expect an acknowledgment within a week. Once a fix is available we
will coordinate disclosure with you and credit you in the release notes
unless you prefer otherwise.

## Scope notes

Mirror Bridge generates and compiles native code. Two boundaries are worth
understanding when assessing impact:

- The **CLI** executes the system's C++ compiler on headers you point it at;
  running it on untrusted source code is equivalent to compiling untrusted
  code.
- **Generated modules** are native extensions: like any pybind11/nanobind
  module, they run with full process privileges inside the host interpreter.
  Type-confusion between bound classes (e.g. passing the wrong wrapper type
  to a bound method) is undefined behavior in the current design, not a
  sandboxed error — do not expose bound modules to adversarial callers.
