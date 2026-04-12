{
  "targets": [
    {
      "target_name": "exception_test",
      "sources": ["exception_binding.cpp"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "../../..",
        "."
      ],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions"],
      "cflags_cc": [
        "-std=c++2c",
        "-freflection",
        "-freflection-latest",
        "-stdlib=libc++",
        "-fPIC"
      ],
      "ldflags": [
        "-stdlib=libc++",
        "-L/usr/local/lib/aarch64-unknown-linux-gnu",
        "-L/usr/local/lib/x86_64-unknown-linux-gnu",
        "-Wl,-rpath,/usr/local/lib/aarch64-unknown-linux-gnu",
        "-Wl,-rpath,/usr/local/lib/x86_64-unknown-linux-gnu"
      ],
      "defines": ["NAPI_DISABLE_CPP_EXCEPTIONS"]
    }
  ]
}
