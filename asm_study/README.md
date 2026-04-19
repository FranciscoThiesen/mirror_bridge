# Assembly study: mirror_bridge vs pybind11

Reproduces the assembly-comparison snippets used in the blog post.

## What's here

| File                 | Purpose                                              |
|----------------------|------------------------------------------------------|
| `geometry_min.hpp`   | A minimal `demo::PointCloud` with one hot method     |
| `bind_mb.cpp`        | mirror_bridge binding (one line: `bind_class<>`)     |
| `bind_pybind.cpp`    | pybind11 binding for the same class                  |
| `build.sh`           | Compiles both to `-O3` assembly, inside the Docker container |

## Reproduce

```bash
# from repo root
docker run --rm -v $(pwd):/workspace -w /workspace/asm_study \
    mirror_bridge:latest ./build.sh
```

This emits `bind_mb.s` and `bind_pybind.s` alongside the sources.

## What to look at

- **`bind_mb.s`**: search for `invoke_with_n_args` — the generated
  dispatcher has the `get_center` reduction loop inlined directly
  into the Python-facing function. There is *no* standalone
  `PointCloud::get_center` symbol in the module.

- **`bind_pybind.s`**: search for `cpp_function::initialize…cl…` —
  pybind11's dispatcher makes four `bl`/`blr` calls (type_caster ctor,
  load_impl, indirect member-fn-ptr call, cast_impl) before reaching
  user code. `demo::PointCloud::get_center` is emitted as its own
  symbol and called indirectly.

See the blog post "The assembly receipt" section for the annotated
snippets and what this means for hot-path performance.
