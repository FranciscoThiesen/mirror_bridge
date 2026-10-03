# Running mirror_bridge in production with two independent teams

This guide is for the case where a C++ team owns a library and a Python team
consumes it, and the two do not coordinate daily. Generating a module is the
easy half of that. The hard half is making sure the Python team finds out
when the C++ team changes something, before production does.

## The problem

A binding generator removes the glue. It does not, by itself, tell anyone
what the glue currently exposes.

So the default failure looks like this. On Monday a C++ engineer renames
`PointCloud::compute_normals` to `estimate_normals`, because nothing in
their repository mentions the old name. CI is green: it builds C++ and runs
C++ tests. The binding still generates, because it generates from whatever
the header says. On Thursday a Python service raises `AttributeError` in
production.

Nothing in that sequence was careless. The information simply never left the
C++ repository.

## What makes this solvable

Every module exposes `__mirror_bridge_stubs__()`, filled in during
`bind_class` from the same reflection data the binding itself is generated
from. `mirror_bridge generate --stubs` writes it next to the module as a
`.pyi`.

Three properties make that file usable as a contract rather than a
convenience:

- **It cannot drift.** It is not parsed back out of a built module or
  maintained by hand. The binding and the stub come from one pass over the
  same reflected declarations, so a stub that disagrees with the binding is
  not a state the tool can reach.
- **It is byte-reproducible.** Two runs over the same headers produce
  identical bytes, so a comparison has no false positives.
- **It does not depend on the compiler.** clang-p2996 and GCC 16 produce the
  same bytes, so the gate does not break when CI changes toolchain.

A file with those three properties is a lockfile. The rest of this guide is
about treating it like one.

## The workflow

**Commit the stub.** It belongs in the C++ repository, next to the headers
it describes, not in a build directory. One file per module:

```
include/quotes/*.hpp
api/quotes.pyi          <- committed
```

Create it once and commit it:

```bash
mirror_bridge api include/quotes --module quotes --update
git add api/quotes.pyi
```

**Gate it in CI.** Add one step to the C++ repository's existing build job:

```yaml
- name: Python surface unchanged
  run: mirror_bridge api include/quotes --module quotes --check
```

`--check` exits 1 when a name disappeared or changed signature. It does not
fail on additions, because nobody's import breaks when a method appears.

**Read the diff in review.** When the gate fires, the author sees what the
other team is about to lose:

```
API surface of 'quotes' changed (baseline api/quotes.pyi):

  removed — importers raise AttributeError:
      Book.best

  changed — importers may pass the wrong type:
      Book.add
          was: (self, q: Quote) -> None
          now: (self, q: Quote, qty: int) -> None

  added — safe for existing importers:
      Book.best_bid

  2 breaking changes, 1 additive.
  Accept with: mirror_bridge api include/quotes --module quotes --update
```

The C++ engineer renaming a method now learns, in their own pull request,
that a Python caller depends on the old name. That is the whole point: the
information moves at review time instead of at runtime.

**Accept deliberately.** A breaking change is often correct. Re-run with
`--update` and commit the new baseline in the same pull request. The
stub diff is then part of the review, and `git log api/quotes.pyi` is the
history of the Python surface.

## Giving the Python team something to build against

The committed stub is also the Python team's type information, and they can
have it without building the C++ at all. Ship it in a stub-only package:

```
quotes-stubs/
    quotes-stubs/__init__.pyi   <- the committed api/quotes.pyi
    pyproject.toml
```

Their editor and their type checker then work against the real surface, and
`mypy` fails in their repository when they call something that no longer
exists, without them compiling a line of C++.

## Versioning

Treat the stub the way you treat any published interface.

- A removed or changed name is a major version bump for the module.
- An added name is a minor bump.
- `--check` tells you which you are making, so the decision does not depend
  on anyone remembering.

If the C++ library already has a version, the useful discipline is that the
Python surface version changes only when the stub changes. A C++ refactor
that leaves the stub byte-identical cannot break the Python team, and the
gate proves it rather than asserting it.

## What this does not cover

Be clear about the boundaries, because a gate that is trusted beyond its
remit is worse than no gate.

- **Behaviour is not in the stub.** A method that keeps its signature and
  changes what it returns passes the gate. The stub is an interface
  contract, not a test suite. The Python team still needs their own tests.
- **Only what was bound is in the stub.** If a name was never bound, its
  removal is invisible here. `generate` reports what it could not bind at
  the end of a run, so read that too.
- **`mirror_bridge diff` is a different, weaker thing.** It compares header
  text, misses a class renamed inside an indented namespace, and fires on
  private members that Python never sees. Use `api` for the Python contract
  and leave `diff` for a quick local look at header churn.
- **ABI is a separate problem.** If the Python team links against a
  prebuilt `.so` rather than building from headers, the standard library and
  compiler have to match. The stub says nothing about that.

## Summary

| | |
|---|---|
| Artifact | `api/<module>.pyi`, committed to the C++ repository |
| Create | `mirror_bridge api <src> --module <m> --update` |
| Gate | `mirror_bridge api <src> --module <m> --check` in C++ CI |
| Fails on | a removed name, or a changed signature |
| Passes on | additions, and any C++ change the Python surface does not see |
| Accept a change | re-run with `--update`, commit the stub in the same PR |
