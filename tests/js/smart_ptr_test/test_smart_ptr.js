// Smart pointer support: unique_ptr/shared_ptr members, params, returns,
// and abstract pointees (null round-trip only).
const mod = require('../../../build/smart_ptr_test_js.node');

let pass = 0, fail = 0;
function check(name, cond) {
    if (cond) { pass++; console.log(`  PASS: ${name}`); }
    else { fail++; console.log(`  FAIL: ${name}`); }
}

const mgr = new mod.ResourceMgr();

check('null unique_ptr reads as null-ish name', mgr.get_unique_name() === 'null');
check('null shared_ptr reads as null-ish name', mgr.get_shared_name() === 'null');

// Object -> smart pointer param (deep-copy semantics)
mgr.set_unique({ name: 'alpha', value: 1 });
check('unique_ptr param materialized from object', mgr.get_unique_name() === 'alpha');

mgr.set_shared({ name: 'beta', value: 2 });
check('shared_ptr param materialized from object', mgr.get_shared_name() === 'beta');

// null -> reset
mgr.set_shared(null);
check('null resets shared_ptr', mgr.get_shared_name() === 'null');

// Smart pointer return: deref'd to a bound value
const d = mgr.make_shared_data('gamma', 3);
check('shared_ptr return yields usable object', d !== null && d.name === 'gamma' && d.value === 3);

// Abstract pointee: compiles, defaults to no shape
const holder = new mod.ShapeHolder();
check('abstract-pointee holder constructs', holder.has_shape() === false);

console.log('');
if (fail === 0) {
    console.log(`All ${pass} JS smart pointer tests passed!`);
} else {
    console.log(`${fail} JS smart pointer tests FAILED`);
    process.exit(1);
}
