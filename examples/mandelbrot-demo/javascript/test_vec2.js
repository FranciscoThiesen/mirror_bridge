// Test Vec2 JavaScript binding - Multi-language demo

const vec2 = require('../build/vec2.node');
const assert = require('assert');

console.log("╔══════════════════════════════════════════╗");
console.log("║      Vec2 · JavaScript Binding Test      ║");
console.log("╚══════════════════════════════════════════╝");
console.log();

// Create vectors
const a = new vec2.Vec2(3.0, 4.0);
const b = new vec2.Vec2(1.0, 0.0);

console.log(`  a = Vec2(${a.x}, ${a.y})`);
console.log(`  b = Vec2(${b.x}, ${b.y})`);
console.log();

// Test methods
console.log("  ┌─────────────────────────────────────┐");
console.log(`  │ a.length()      = ${a.length().toString().padEnd(18)} │`);
console.log(`  │ a.dot(b)        = ${a.dot(b).toString().padEnd(18)} │`);

const n = a.normalized();
console.log(`  │ a.normalized()  = (${n.x.toFixed(2)}, ${n.y.toFixed(2)})          │`);

const scaled = a.scale(2.0);
console.log(`  │ a.scale(2.0)    = (${scaled.x.toFixed(1)}, ${scaled.y.toFixed(1)})           │`);

const added = a.add(b);
console.log(`  │ a.add(b)        = (${added.x.toFixed(1)}, ${added.y.toFixed(1)})            │`);

const dist = a.distance_to(b);
console.log(`  │ a.distance_to(b)= ${dist.toFixed(4).padEnd(18)} │`);
console.log("  └─────────────────────────────────────┘");
console.log();

// Verify correctness
assert(Math.abs(a.length() - 5.0) < 0.0001, "length() failed");
assert(Math.abs(a.dot(b) - 3.0) < 0.0001, "dot() failed");
assert(Math.abs(n.x - 0.6) < 0.0001, "normalized() x failed");
assert(Math.abs(n.y - 0.8) < 0.0001, "normalized() y failed");

console.log("  ✓ All assertions passed!");
console.log();
