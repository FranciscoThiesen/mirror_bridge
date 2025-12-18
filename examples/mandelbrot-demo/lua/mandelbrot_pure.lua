-- Pure Lua Mandelbrot implementation (no C++ bindings)

local function escape_time(real, imag, max_iter)
    -- Compute escape time for a single point with smooth coloring
    local zr, zi = 0.0, 0.0
    local iter = 0

    while zr * zr + zi * zi <= 4.0 and iter < max_iter do
        local temp = zr * zr - zi * zi + real
        zi = 2.0 * zr * zi + imag
        zr = temp
        iter = iter + 1
    end

    if iter == max_iter then
        return max_iter
    end

    -- Smooth coloring
    local log_zn = math.log(zr * zr + zi * zi) / 2.0
    local nu = math.log(log_zn / math.log(2.0)) / math.log(2.0)
    return iter + 1.0 - nu
end

local function iter_to_rgb(iter_val, max_iter)
    -- Convert iteration count to RGB color
    if iter_val >= max_iter then
        return 0, 0, 0
    end

    local t = iter_val / max_iter
    local hue = (t * 5.0 + 0.6) % 1.0
    local sat = 0.85
    local val = t < 0.02 and t / 0.02 or 1.0

    -- HSV to RGB
    local hi = math.floor(hue * 6.0) % 6
    local f = hue * 6.0 - hi
    local p = val * (1.0 - sat)
    local q = val * (1.0 - f * sat)
    local tv = val * (1.0 - (1.0 - f) * sat)

    local rd, gd, bd
    if hi == 0 then
        rd, gd, bd = val, tv, p
    elseif hi == 1 then
        rd, gd, bd = q, val, p
    elseif hi == 2 then
        rd, gd, bd = p, val, tv
    elseif hi == 3 then
        rd, gd, bd = p, q, val
    elseif hi == 4 then
        rd, gd, bd = tv, p, val
    else
        rd, gd, bd = val, p, q
    end

    return math.floor(rd * 255), math.floor(gd * 255), math.floor(bd * 255)
end

local function render_mandelbrot(width, height, max_iter, x_min, x_max, y_min, y_max)
    -- Render Mandelbrot to RGB pixel table - THE HOT LOOP
    local pixels = {}

    local x_scale = (x_max - x_min) / width
    local y_scale = (y_max - y_min) / height

    for py = 0, height - 1 do
        local imag = y_min + py * y_scale
        for px = 0, width - 1 do
            local real = x_min + px * x_scale
            local iter_val = escape_time(real, imag, max_iter)
            local r, g, b = iter_to_rgb(iter_val, max_iter)
            pixels[#pixels + 1] = r
            pixels[#pixels + 1] = g
            pixels[#pixels + 1] = b
        end
    end

    return pixels
end

local function to_ppm(width, height, pixels)
    -- Convert pixel data to PPM string
    local parts = {string.format("P3\n%d %d\n255\n", width, height)}

    for i = 1, #pixels, 3 do
        local idx = (i - 1) / 3
        parts[#parts + 1] = string.format("%d %d %d", pixels[i], pixels[i+1], pixels[i+2])
        if idx % width == width - 1 then
            parts[#parts + 1] = "\n"
        else
            parts[#parts + 1] = " "
        end
    end

    return table.concat(parts)
end

-- Export for use as module
local M = {
    escape_time = escape_time,
    iter_to_rgb = iter_to_rgb,
    render_mandelbrot = render_mandelbrot,
    to_ppm = to_ppm
}

-- For benchmarking when run directly
if arg and arg[0] and arg[0]:match("mandelbrot_pure") then
    local width, height = 800, 600
    local max_iter = 256
    local x_min, x_max = -2.5, 1.0
    local y_min, y_max = -1.25, 1.25

    print(string.format("Pure Lua Mandelbrot (%dx%d, %d iterations)", width, height, max_iter))

    local start = os.clock()
    local pixels = render_mandelbrot(width, height, max_iter, x_min, x_max, y_min, y_max)
    local elapsed = os.clock() - start

    print(string.format("Render time: %.3f seconds", elapsed))
    print(string.format("Pixels: %d", #pixels / 3))
end

return M
