// Class-typed values are identity-checked at the JavaScript boundary.
//
// Before the check, napi_unwrap answered "yes, this object was wrapped"
// without saying wrapped as what, so any bound object satisfied any
// class-typed parameter and any receiver: Curve.prototype.at.call(label, 2)
// returned 4.05e-322, a denormal read out of the std::string's bytes.
// These tests pin both halves of the fix: a wrong class throws a TypeError,
// and the derived-to-base conversions that are supposed to work still do,
// now including a base that is not at offset zero.

// Resolution order: NODE_PATH (set by ctest to the CMake build tree),
// then the bash harness output
let ts;
try {
    ts = require('type_safety_js');
} catch (e) {
    ts = require('../../../build/type_safety_js.node');
}

let passed = 0;

function ok(message) {
    passed += 1;
    console.log(`  PASS: ${message}`);
}

function assert(condition, message) {
    if (!condition) throw new Error(message);
}

// Returns the TypeError's message; fails if the call was accepted.
function rejects(fn, label) {
    try {
        fn();
    } catch (e) {
        assert(e instanceof TypeError, `${label} threw ${e.constructor.name}, not TypeError`);
        return e.message;
    }
    throw new Error(`${label} was accepted`);
}

// The dangerous case: a real wrapper, so every null check passes, but the
// wrong class.
const curve = new ts.Curve();
const label = new ts.Label();

assert(curve.at(2.0) === 0.1, "correct method call broke");
assert(label.size() === label.tag.length, "correct method call broke");
ok("the correct type still converts");

let message = rejects(() => ts.Curve.prototype.at.call(label, 2.0),
                      "Curve.prototype.at.call(Label)");
assert(message.includes("Curve"), `error does not name the expected class: ${message}`);
assert(message.includes("Label"), `error does not name the actual class: ${message}`);
ok("a receiver of the wrong class throws, naming both classes");

for (const [name, value] of [["number", 42], ["string", "text"], ["plain object", {}]]) {
    rejects(() => ts.Curve.prototype.at.call(value, 2.0), `Curve.prototype.at.call(${name})`);
}
ok("a receiver that is not a wrapper throws");

const portfolio = new ts.Portfolio();
assert(portfolio.book(new ts.Curve()) === 0.05, "correct argument broke");

message = rejects(() => portfolio.book(label), "Portfolio.book(Label)");
assert(message.includes("Curve"), `error does not name the expected class: ${message}`);
assert(message.includes("Label"), `error does not name the actual class: ${message}`);
ok("an unrelated bound class is rejected as an argument, not reinterpreted");

rejects(() => portfolio.label_size(curve), "Portfolio.label_size(Curve)");
ok("the rejection runs in both directions");

// A property accessor can be lifted off the prototype and called on anything.
const rate = Object.getOwnPropertyDescriptor(ts.Curve.prototype, 'rate');
rejects(() => rate.get.call(label), "Curve.rate getter on a Label");
rejects(() => rate.set.call(label, 1.0), "Curve.rate setter on a Label");
ok("a hijacked accessor throws instead of reading foreign bytes");

// Base is at offset 0, so this worked before the check. Regression guard.
assert(portfolio.take_base(new ts.Derived()) === 7, "derived-to-base conversion broke");
ok("derived-to-base still converts (offset 0)");

// Second is NOT at offset 0: before the check this returned First's bytes.
assert(portfolio.take_second(new ts.Both()) === 22222.0,
       "conversion to a non-first base landed on the wrong subobject");
ok("conversion to a non-first base lands on the right subobject");

// The downcast direction: an unrelated base is not a Second.
rejects(() => portfolio.take_second(new ts.First()), "Portfolio.take_second(First)");
ok("an unrelated base is still refused");

// Property assignment goes through the same conversion.
rejects(() => { portfolio.held = label; }, "Portfolio.held = Label");
portfolio.held = new ts.Curve();
assert(portfolio.held.rate === 0.05, "class-typed property assignment broke");
ok("class-typed property assignment is checked");

console.log();
console.log(`All ${passed} JavaScript type safety tests passed`);
