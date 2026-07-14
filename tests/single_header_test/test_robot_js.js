// Test single-header JavaScript binding
const path = require('path');
const assert = require('assert');

// Resolution order: NODE_PATH (set by ctest to the CMake build tree),
// then the bash harness output in <repo>/build
let robot;
try {
    robot = require('robot_js');
} catch (e) {
    robot = require(path.join(__dirname, '..', '..', 'build', 'robot_js'));
}

console.log('=== JavaScript Single-Header Test ===\n');

// Test 1: Create robot
console.log('Test 1: Creating robot...');
const r = new robot.Robot();
r.name = "RoboBot";
r.battery = 100.0;
assert.strictEqual(r.name, "RoboBot");
assert.strictEqual(r.battery, 100.0);
console.log(`  ✓ Robot created: ${r.name}, battery: ${r.battery}%`);

// Test 2: Move forward
console.log('\nTest 2: Moving forward...');
r.move_forward(50.0);
assert.strictEqual(r.battery, 95.0);
console.log(`  ✓ Moved 50m, battery: ${r.battery}%`);

// Test 3: Get status
console.log('\nTest 3: Get status...');
const status = r.get_status();
assert(status.includes("RoboBot"));
assert(status.includes("95"));
console.log(`  ✓ Status: ${status}`);

// Test 4: Commands
console.log('\nTest 4: Add commands...');
r.commands = ["forward", "left", "forward"];
const count = r.command_count();
assert.strictEqual(count, 3);
console.log(`  ✓ Added ${count} commands`);

// Test 5: Reset
console.log('\nTest 5: Reset...');
r.reset();
assert.strictEqual(r.battery, 100.0);
assert.strictEqual(r.command_count(), 0);
console.log(`  ✓ Reset: battery=${r.battery}%, commands=${r.command_count()}`);

console.log('\n' + '='.repeat(40));
console.log('✓ All JavaScript single-header tests passed!');
console.log('='.repeat(40));
