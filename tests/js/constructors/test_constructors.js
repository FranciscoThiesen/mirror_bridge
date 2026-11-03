// A constructor call builds the object it was asked for, or throws a
// TypeError that says why.
//
// Before, arguments that no constructor accepted were dropped in favour of
// the default constructor: new Rect(2) was a 0 x 0 rectangle and
// new Point2(3, 4) was (0, 0), with no error, and a call with more than
// sixteen arguments read sixteen of them and then defaulted too. A deleted
// overload, an abstract class that declares a constructor, and a constructor
// taking a non-const reference each failed the whole module to compile, so
// this file could not have been loaded at all.

// Resolution order: NODE_PATH (set by ctest to the CMake build tree), then
// the bash harness output
let m;
try {
    m = require('constructors_js');
} catch (e) {
    m = require('../../../build/constructors_js.node');
}

let passed = 0;

function ok(message) {
    passed += 1;
    console.log(`  PASS: ${message}`);
}

function assert(condition, message) {
    if (!condition) throw new Error(message);
}

// The TypeError's message; fails if the call was accepted.
function rejects(fn) {
    try {
        fn();
    } catch (e) {
        assert(e instanceof TypeError, `threw ${e.constructor.name}, not TypeError: ${e.message}`);
        return e.message;
    }
    throw new Error('the call was accepted');
}

function mentions(message, ...needles) {
    for (const needle of needles) {
        assert(message.includes(needle), `expected '${needle}' in: ${message}`);
    }
}

assert(new m.Rect(2, 3).area() === 6, 'new Rect(2, 3)');
assert(new m.Rect().area() === 0, 'new Rect()');
ok('a constructor builds from its arguments, and with none default-constructs');

// Arguments no constructor accepts are refused, never defaulted
mentions(rejects(() => new m.Rect(2)), 'Rect', 'no constructor takes 1 argument', 'constructors take 0 or 2');
mentions(rejects(() => new m.Rect(1, 2, 3)), 'Rect', 'no constructor takes 3 arguments');
mentions(rejects(() => new m.Rect(...Array(20).fill(1))), 'Rect', 'no constructor takes 20 arguments');
ok('a count no constructor takes is refused, naming the counts that work');

mentions(rejects(() => new m.Rect('a', 'b')), 'Rect', 'argument 1');
mentions(rejects(() => new m.Rect(2, undefined)), 'Rect', 'argument 2');
mentions(rejects(() => new m.Cell('a', 2)), 'Cell', 'argument 1');
ok('a value that will not convert is refused, naming the argument');

mentions(rejects(() => new m.Point2(3, 4)), 'Point2', 'takes no constructor arguments');
const p = new m.Point2();
assert(p.x === 0 && p.y === 0, 'new Point2()');
ok('an aggregate refuses arguments and still default-constructs');

mentions(rejects(() => new m.Cell()), 'Cell', 'no default constructor', 'constructors take 2');
const cell = new m.Cell(4, 5);
assert(cell.row === 4 && cell.col === 5, 'new Cell(4, 5)');
ok('a class with no default constructor asks for its arguments');

const from = new m.Point2();
const to = new m.Point2();
to.x = 3;
to.y = 4;
assert(new m.Segment(from, to).length() === 5, 'new Segment(from, to)');
ok('a bound class as a constructor argument');

// Constructors of one arity are tried in declaration order
const numeric = new m.Tagged(2, 3);
assert(numeric.value === 6 && numeric.tag === 'numeric', 'new Tagged(2, 3)');
assert(new m.Tagged('a', 'b').tag === 'ab', "new Tagged('a', 'b')");
mentions(rejects(() => new m.Tagged(true, 'x')),
         'Tagged', 'no constructor taking 2 arguments accepts', 'boolean, string');
ok('of two constructors taking two arguments, the one that converts wins');

// Constructors that cannot be called are not offered
assert(new m.Celsius(36.5).degrees === 36.5, 'new Celsius(36.5)');
assert(new m.Celsius(36).degrees === 36, 'an integer reached the deleted Celsius(int)');
ok('a deleted overload is not offered');

mentions(rejects(() => new m.Gauge(5)), 'Gauge', 'abstract');
mentions(rejects(() => new m.Gauge()), 'Gauge', 'abstract');
const gauge = new m.FixedGauge(5);
assert(gauge.measure() === 6 && gauge.doubled_tag() === 10, 'new FixedGauge(5)');
ok('an abstract class with a constructor binds, refuses construction, and its subclass works');

assert(new m.Widget(4).level === 4 && new m.Widget().level === 0, 'new Widget(4), new Widget()');
mentions(rejects(() => new m.Widget(new m.Config())), 'Widget', 'argument 1');
ok('a constructor taking a non-const reference is not offered; the others still are');

mentions(rejects(() => new m.Line(new m.Cell(0, 1), new m.Cell(0, 4))), 'Line', 'takes no constructor arguments');
assert(new m.Line().length === 0, 'new Line()');
ok('a constructor taking a class with no default constructor is not offered');

mentions(rejects(() => new m.View()), 'View', 'cannot be constructed from JavaScript');
mentions(rejects(() => new m.View(new m.Config())), 'View', 'cannot be constructed from JavaScript');
ok('a class with nothing callable says so');

const seventeen = Array.from({ length: 17 }, (_, i) => i + 1);
assert(new m.Big(...seventeen).sum === 153, 'new Big(1..17)');
mentions(rejects(() => new m.Big(...seventeen.slice(0, 16))), 'Big', 'no constructor takes 16 arguments');
ok('a constructor with seventeen parameters, more than the old sixteen slots');

const ints = new m.Mixed(5, 6);
assert(ints.tag === 'ints' && ints.a === 5 && ints.b === 6, 'new Mixed(5, 6)');
assert(new m.Mixed('x', [7, 8]).tag === 'x', "new Mixed('x', [7, 8])");
ok('of two constructors taking two arguments, the one that converts wins, either way round');

assert(new m.Linked().value === 0, 'new Linked()');
mentions(rejects(() => new m.Linked(1, null)), 'Linked', 'takes no constructor arguments');
ok('a constructor taking a raw pointer is not offered');

assert(new m.Meas(1 / 3, 1).value === 1 / 3 && new m.Meas('ab', [1, 2, 3]).value === 5, 'Meas');
ok('overloads with a borrowed string and with numbers each receive their own calls');

// A throwing constructor raises the C++ message, as an Error
let threw = null;
try {
    new m.Checked(-1);
} catch (e) {
    threw = e;
}
assert(threw && threw.message.includes('non-negative'), 'Checked(-1) did not raise the C++ message');
assert(new m.Checked(3).value === 3, 'new Checked(3)');
ok('a C++ exception from a constructor becomes a JavaScript error');

console.log('');
console.log(`All ${passed} JavaScript constructor tests passed!`);
