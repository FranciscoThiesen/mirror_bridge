FROM ubuntu:22.04

# Non-interactive apt for the whole image build.
ENV DEBIAN_FRONTEND=noninteractive

# Essential build tools + Python + Node.js + Lua + C++ deps used by
# downstream binding targets (Eigen, fmt, jsoncpp, nanoflann).
# Kept as a single RUN so the image cache stays useful and so the
# cleanup of /var/lib/apt/lists is correctly scoped.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential sudo ninja-build vim cmake git wget ca-certificates \
        python3 python3-pip python3-dev \
        nodejs npm \
        lua5.4 liblua5.4-dev \
        libeigen3-dev libfmt-dev libjsoncpp-dev libnanoflann-dev \
 && ln -sf /usr/bin/python3 /usr/local/bin/python \
 && ln -sf /usr/bin/pip3 /usr/local/bin/pip \
 && rm -rf /var/lib/apt/lists/*

# Install Node.js native addon tools
RUN npm install -g node-gyp node-addon-api

# Install Jupyter for interactive notebooks
RUN pip3 install --no-cache-dir --break-system-packages jupyter notebook ipython

# Build and install clang-p2996 with reflection support AND libcxx
# This branch implements the C++26 reflection proposal (P2996)
WORKDIR /opt

RUN git clone --depth 1 --branch p2996 https://github.com/bloomberg/clang-p2996.git

# Build clang first
WORKDIR /opt/clang-p2996/build
RUN cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS="clang" \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    ../llvm && \
    ninja && \
    ninja install

# Set up compiler environment variables
ENV CC=/usr/local/bin/clang
ENV CXX=/usr/local/bin/clang++

# Now build libc++ with reflection support using the newly built clang
# This is done separately to ensure the <meta> header is properly installed
WORKDIR /opt/clang-p2996/runtimes-build
RUN cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind" \
    -DLIBCXX_ENABLE_REFLECTION=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_C_COMPILER=/usr/local/bin/clang \
    -DCMAKE_CXX_COMPILER=/usr/local/bin/clang++ \
    ../runtimes && \
    ninja && \
    ninja install

# Clean up build artifacts to reduce image size
RUN rm -rf /opt/clang-p2996

# Configure library paths
RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/libc++.conf && ldconfig

# Set LD_LIBRARY_PATH
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Work around a clang-p2996 + fmt 8.1 incompatibility. FMT_STRING calls in
# fmt/format-inl.h bigint printing fail consteval under the reflection
# compiler. These calls are only reachable from debug-print paths; replacing
# them with runtime-parsed string_view has no functional impact but lets
# header-only fmt compile cleanly under clang-p2996.
RUN sed -i \
    -e 's|format_to(out, FMT_STRING("{:x}"), value)|format_to(out, fmt::string_view("{:x}"), value)|' \
    -e 's|format_to(out, FMT_STRING("{:08x}"), value)|format_to(out, fmt::string_view("{:08x}"), value)|' \
    -e 's|format_to(out, FMT_STRING("p{}"),|format_to(out, fmt::string_view("p{}"),|' \
    /usr/include/fmt/format-inl.h

# Verify the <meta> header is installed
RUN echo '#include <meta>' > /tmp/test.cpp && \
    echo 'int main() { return 0; }' >> /tmp/test.cpp && \
    clang++ -std=c++2c -freflection -freflection-latest -stdlib=libc++ /tmp/test.cpp -o /tmp/test && \
    rm /tmp/test.cpp /tmp/test && \
    echo "SUCCESS: <meta> header is available"

# Create workspace directory
WORKDIR /workspace

# Default command drops into bash for interactive development
CMD ["/bin/bash"]
