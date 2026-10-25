// The harness builds every module into build/, the same way every other
// JavaScript test here finds its addon.
const m = require('../../../build/const_members_js.node');

console.log("=== const and non-assignable members, JavaScript ===");
const assert = require('assert');

const s = new m.Sensor();
assert.strictEqual(s.id, 42, "const integer member should read its C++ initialiser");
assert.strictEqual(s.model, "MB-1000", "const string member should read");
console.log("  ok  const members read");

s.reading = 2.5;
assert.strictEqual(s.reading, 2.5, "a non-const member beside a const one still writes");
assert.ok(Math.abs(s.scaled(4.0) - 10.0) < 1e-9, "a method sees the written value");
console.log("  ok  the writable member beside them still works");

assert.throws(() => { s.id = 1; }, /not assignable/,
              "writing a const member should throw and say why");
assert.strictEqual(s.id, 42, "and the value must be unchanged");
console.log("  ok  writing a const member is refused by name");

// Fixed has only Fixed(int), so the setter has no local it can default-construct.
const h = new m.Holder();
assert.strictEqual(h.anchor_value(), 7, "a member whose type has no default constructor still reads");
h.adjustable = 9.0;
assert.strictEqual(h.adjustable, 9.0, "and its neighbours are unaffected");
h.reseat(21);
assert.strictEqual(h.anchor_value(), 21, "and C++ can still change it through a method");
console.log("  ok  a member with no default constructor binds as readable");

console.log("PASS: const and non-assignable members (JavaScript)");
