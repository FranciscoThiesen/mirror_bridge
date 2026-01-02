// Test for custom type converter extension point (JavaScript)
// Verifies that user-defined converters work correctly for types
// not natively supported by mirror_bridge

const addon = require('../../../build/pixel_js.node');
const assert = require('assert');

console.log('=== Custom Type Converter Test (JavaScript) ===\n');

// Test 1: Create Pixel with default Color3
console.log('Test 1: Default construction...');
const p = new addon.Pixel();
assert.strictEqual(p.x, 0);
assert.strictEqual(p.y, 0);
let color = p.get_color();
assert.deepStrictEqual(color, [0, 0, 0], `Expected [0,0,0], got ${JSON.stringify(color)}`);
console.log(`  Default color: ${JSON.stringify(color)}`);

// Test 2: Get Color3 as JavaScript array
console.log('\nTest 2: Color3 returned as array...');
p.x = 10;
p.y = 20;
color = p.get_color();
assert(Array.isArray(color), `Expected array, got ${typeof color}`);
assert.strictEqual(color.length, 3, `Expected 3 elements, got ${color.length}`);
console.log(`  Color type: ${Array.isArray(color) ? 'Array' : typeof color}, value: ${JSON.stringify(color)}`);

// Test 3: Set Color3 from array
console.log('\nTest 3: Set color from array...');
p.set_color([255, 128, 64]);
color = p.get_color();
assert.deepStrictEqual(color, [255, 128, 64], `Expected [255,128,64], got ${JSON.stringify(color)}`);
console.log(`  Set from [255, 128, 64]: ${JSON.stringify(color)}`);

// Test 4: Set Color3 from object
console.log('\nTest 4: Set color from object...');
p.set_color({ r: 100, g: 150, b: 200 });
color = p.get_color();
assert.deepStrictEqual(color, [100, 150, 200], `Expected [100,150,200], got ${JSON.stringify(color)}`);
console.log(`  Set from {r:100, g:150, b:200}: ${JSON.stringify(color)}`);

// Test 5: Method returning Color3 (inverted_color)
console.log('\nTest 5: Method returning custom type...');
p.set_color([100, 50, 25]);
const inverted = p.inverted_color();
assert.deepStrictEqual(inverted, [155, 205, 230], `Expected [155,205,230], got ${JSON.stringify(inverted)}`);
console.log(`  Original: [100, 50, 25], Inverted: ${JSON.stringify(inverted)}`);

// Test 6: Color3 as property (direct field access)
console.log('\nTest 6: Color3 as direct property...');
p.color = [10, 20, 30];
const directColor = p.color;
assert.deepStrictEqual(directColor, [10, 20, 30], `Expected [10,20,30], got ${JSON.stringify(directColor)}`);
console.log(`  Direct property access: ${JSON.stringify(directColor)}`);

// Test 7: Round-trip consistency
console.log('\nTest 7: Round-trip consistency...');
const testColors = [
    [0, 0, 0],
    [255, 255, 255],
    [128, 64, 32],
    [1, 2, 3],
];
for (const expected of testColors) {
    p.set_color(expected);
    const actual = p.get_color();
    assert.deepStrictEqual(actual, expected,
        `Round-trip failed: ${JSON.stringify(expected)} -> ${JSON.stringify(actual)}`);
}
console.log(`  All ${testColors.length} round-trip tests passed`);

console.log('\n' + '='.repeat(40));
console.log('All custom type converter tests passed!');
console.log('='.repeat(40));
