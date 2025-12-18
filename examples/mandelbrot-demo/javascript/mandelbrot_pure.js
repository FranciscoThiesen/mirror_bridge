// Pure JavaScript Mandelbrot implementation (no C++ bindings)

function escapeTime(real, imag, maxIter) {
    // Compute escape time for a single point with smooth coloring
    let zr = 0.0, zi = 0.0;
    let iter = 0;

    while (zr * zr + zi * zi <= 4.0 && iter < maxIter) {
        const temp = zr * zr - zi * zi + real;
        zi = 2.0 * zr * zi + imag;
        zr = temp;
        iter++;
    }

    if (iter === maxIter) {
        return maxIter;
    }

    // Smooth coloring
    const logZn = Math.log(zr * zr + zi * zi) / 2.0;
    const nu = Math.log(logZn / Math.log(2.0)) / Math.log(2.0);
    return iter + 1.0 - nu;
}

function iterToRgb(iterVal, maxIter) {
    // Convert iteration count to RGB color
    if (iterVal >= maxIter) {
        return [0, 0, 0];
    }

    const t = iterVal / maxIter;
    const hue = (t * 5.0 + 0.6) % 1.0;
    const sat = 0.85;
    const val = t < 0.02 ? t / 0.02 : 1.0;

    // HSV to RGB
    const hi = Math.floor(hue * 6.0) % 6;
    const f = hue * 6.0 - hi;
    const p = val * (1.0 - sat);
    const q = val * (1.0 - f * sat);
    const tv = val * (1.0 - (1.0 - f) * sat);

    let rd, gd, bd;
    switch (hi) {
        case 0: rd = val; gd = tv;  bd = p;   break;
        case 1: rd = q;   gd = val; bd = p;   break;
        case 2: rd = p;   gd = val; bd = tv;  break;
        case 3: rd = p;   gd = q;   bd = val; break;
        case 4: rd = tv;  gd = p;   bd = val; break;
        default: rd = val; gd = p;  bd = q;   break;
    }

    return [Math.floor(rd * 255), Math.floor(gd * 255), Math.floor(bd * 255)];
}

function renderMandelbrot(width, height, maxIter, xMin, xMax, yMin, yMax) {
    // Render Mandelbrot to RGB pixel array - THE HOT LOOP
    const pixels = new Uint8Array(width * height * 3);

    const xScale = (xMax - xMin) / width;
    const yScale = (yMax - yMin) / height;

    let idx = 0;
    for (let py = 0; py < height; py++) {
        const imag = yMin + py * yScale;
        for (let px = 0; px < width; px++) {
            const real = xMin + px * xScale;
            const iterVal = escapeTime(real, imag, maxIter);
            const [r, g, b] = iterToRgb(iterVal, maxIter);
            pixels[idx++] = r;
            pixels[idx++] = g;
            pixels[idx++] = b;
        }
    }

    return pixels;
}

function toPpm(width, height, pixels) {
    // Convert pixel data to PPM string
    let ppm = `P3\n${width} ${height}\n255\n`;

    for (let i = 0; i < pixels.length; i += 3) {
        const idx = i / 3;
        ppm += `${pixels[i]} ${pixels[i+1]} ${pixels[i+2]}`;
        ppm += (idx % width === width - 1) ? '\n' : ' ';
    }

    return ppm;
}

// Export for use as module
module.exports = {
    escapeTime,
    iterToRgb,
    renderMandelbrot,
    toPpm
};

// For benchmarking when run directly
if (require.main === module) {
    const width = 800, height = 600;
    const maxIter = 256;
    const xMin = -2.5, xMax = 1.0;
    const yMin = -1.25, yMax = 1.25;

    console.log(`Pure JavaScript Mandelbrot (${width}x${height}, ${maxIter} iterations)`);

    const start = performance.now();
    const pixels = renderMandelbrot(width, height, maxIter, xMin, xMax, yMin, yMax);
    const elapsed = (performance.now() - start) / 1000;

    console.log(`Render time: ${elapsed.toFixed(3)} seconds`);
    console.log(`Pixels: ${pixels.length / 3}`);
}
