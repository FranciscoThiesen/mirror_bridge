// Tests for C++ exception handling in JavaScript bindings
// C++ exceptions should be caught and converted to JavaScript errors

const mod = require('./build/Release/exception_test');

let pass = 0;
let fail = 0;

function test(name, fn) {
    try {
        fn();
        console.log(`  PASS: ${name}`);
        pass++;
    } catch (e) {
        console.log(`  FAIL: ${name} - ${e.message}`);
        fail++;
    }
}

function assert(cond, msg) {
    if (!cond) throw new Error(msg || 'Assertion failed');
}

console.log('Testing C++ exception handling in JavaScript bindings:');

// Test: successful operations
test('safe_divide success', () => {
    const svc = new mod.ThrowingService();
    const result = svc.safe_divide(10.0, 2.0);
    assert(result === 5.0, `Expected 5.0 but got ${result}`);
});

test('safe_sqrt success', () => {
    const svc = new mod.ThrowingService();
    const result = svc.safe_sqrt(4.0);
    assert(result === 2.0, `Expected 2.0 but got ${result}`);
});

// Test: C++ exceptions should become JavaScript errors
test('safe_divide throws on zero', () => {
    const svc = new mod.ThrowingService();
    let caught = false;
    try {
        svc.safe_divide(10.0, 0.0);
    } catch (e) {
        caught = true;
        assert(e.message.includes('division by zero'),
            `Error should mention 'division by zero', got: ${e.message}`);
    }
    assert(caught, 'Expected error but call succeeded');
});

test('safe_sqrt throws on negative', () => {
    const svc = new mod.ThrowingService();
    let caught = false;
    try {
        svc.safe_sqrt(-1.0);
    } catch (e) {
        caught = true;
        assert(e.message.includes('negative'),
            `Error should mention 'negative', got: ${e.message}`);
    }
    assert(caught, 'Expected error but call succeeded');
});

test('set_positive throws on non-positive', () => {
    const svc = new mod.ThrowingService();
    let caught = false;
    try {
        svc.set_positive(-5.0);
    } catch (e) {
        caught = true;
        assert(e.message.includes('positive'),
            `Error should mention 'positive', got: ${e.message}`);
    }
    assert(caught, 'Expected error but call succeeded');
});

// Test: object state preserved after exception
test('object state preserved after exception', () => {
    const svc = new mod.ThrowingService();
    svc.set_positive(42.0);
    assert(svc.value === 42.0, 'Value should be 42.0');

    try { svc.set_positive(-1.0); } catch (e) { /* expected */ }

    assert(svc.value === 42.0, 'Value should still be 42.0 after failed set');
});

console.log('');
if (fail === 0) {
    console.log(`All ${pass} JavaScript exception tests passed!`);
} else {
    console.log(`${fail} of ${pass + fail} tests FAILED`);
    process.exit(1);
}
