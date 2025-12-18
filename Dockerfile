FROM archlinux:latest

# Install essential build tools, Python, Node.js, Lua, and V8 development files
RUN pacman -Syu --noconfirm base-devel sudo ninja vim cmake git python python-pip nodejs npm lua wget ca-certificates

# Install Node.js native addon tools
RUN npm install -g node-gyp node-addon-api

# Install Jupyter for interactive notebooks
RUN pip install --no-cache-dir --break-system-packages jupyter notebook ipython

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
