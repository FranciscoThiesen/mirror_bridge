// Test Vec3 JavaScript bindings for:
// 1. Static methods (hot_loop, add_vectors)
// 2. Methods with const reference parameters (dot)

// Resolution order: NODE_PATH (set by ctest to the CMake build tree),
// then the node-gyp build path (standalone runs), then the bash harness output
let vec3;
try {
    vec3 = require('vec3_js');
} catch (e0) {
    try {
        vec3 = require('./build/Release/vec3.node');
    } catch (e) {
        vec3 = require('../../../build/vec3_js.node');
    }
}

console.log("============================================================");
console.log("Vec3 JavaScript Binding Tests");
console.log("============================================================");
console.log();

let allPassed = true;

function assert(condition, message) {
    if (!condition) {
        console.log(`  FAIL: ${message}`);
        allPassed = false;
        throw new Error(message);
    }
}

function approxEqual(a, b, epsilon = 0.001) {
    return Math.abs(a - b) < epsilon;
}

// Test 1: Basic construction
console.log("Testing basic construction...");
const v = new vec3.Vec3();
assert(v.x === 0 && v.y === 0 && v.z === 0, "Default constructor failed");
console.log("  PASS: Default constructor works");

// Test 2: Setting values
v.x = 1.0;
v.y = 2.0;
v.z = 3.0;
assert(v.x === 1.0 && v.y === 2.0 && v.z === 3.0, "Setting values failed");
console.log("  PASS: Setting values works");

// Test 3: Const reference parameter (dot method)
console.log();
console.log("Testing const reference parameter (dot method)...");
const a = new vec3.Vec3();
a.x = 1.0; a.y = 2.0; a.z = 3.0;
const b = new vec3.Vec3();
b.x = 4.0; b.y = 5.0; b.z = 6.0;

// dot product: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
const dotResult = a.dot(b);
assert(approxEqual(dotResult, 32.0), `dot() returned wrong value: ${dotResult}`);
console.log(`  PASS: a.dot(b) = ${dotResult}`);

// Test 4: length method
console.log();
console.log("Testing length method...");
const v2 = new vec3.Vec3();
v2.x = 3.0; v2.y = 4.0; v2.z = 0.0;
const len = v2.length();
assert(approxEqual(len, 5.0), `length() returned wrong value: ${len}`);
console.log(`  PASS: length() = ${len}`);

// Test 5: Static method (hot_loop)
console.log();
console.log("Testing static method (hot_loop)...");
const hotResult = vec3.Vec3.hot_loop(1000);
assert(hotResult > 0, "hot_loop() returned non-positive value");
console.log(`  PASS: Vec3.hot_loop(1000) = ${hotResult.toFixed(2)}`);

// Test 6: Static method with const ref params returning bound type
console.log();
console.log("Testing static method with const ref params (add_vectors)...");
const vA = new vec3.Vec3();
vA.x = 1.0; vA.y = 2.0; vA.z = 3.0;
const vB = new vec3.Vec3();
vB.x = 4.0; vB.y = 5.0; vB.z = 6.0;

const vSum = vec3.Vec3.add_vectors(vA, vB);
assert(approxEqual(vSum.x, 5.0), `add_vectors x wrong: ${vSum.x}`);
assert(approxEqual(vSum.y, 7.0), `add_vectors y wrong: ${vSum.y}`);
assert(approxEqual(vSum.z, 9.0), `add_vectors z wrong: ${vSum.z}`);
console.log(`  PASS: Vec3.add_vectors(a, b) = (${vSum.x}, ${vSum.y}, ${vSum.z})`);

// Test 7: Verify returned object has working methods
console.log();
console.log("Testing that returned object has working methods...");
const vSumLen = vSum.length();
// length of (5, 7, 9) = sqrt(25 + 49 + 81) = sqrt(155) ≈ 12.45
assert(vSumLen > 12.0 && vSumLen < 13.0, `Returned object length wrong: ${vSumLen}`);
console.log(`  PASS: Returned Vec3 has working methods, length() = ${vSumLen.toFixed(2)}`);

console.log();
console.log("============================================================");
if (allPassed) {
    console.log("ALL TESTS PASSED!");
} else {
    console.log("SOME TESTS FAILED!");
    process.exit(1);
}
console.log("============================================================");
