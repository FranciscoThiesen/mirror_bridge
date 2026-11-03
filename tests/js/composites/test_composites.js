// Composite conversions either deliver the value they were given or refuse
// it, and a container element may be any type a top-level value may be.
//
// Before: a non-null value for a shared_ptr to an abstract class became a
// null pointer (scene.shape = circle cleared the member; area_of(circle)
// received nullptr); a std::array accepted any length and kept nothing, so
// body.pos = [1, 2, 3] read back as uninitialised memory; a std::span
// arrived empty; every member of Collections but frames failed the whole
// module to compile, and frames stored garbage.

// Resolution order: NODE_PATH (set by ctest to the CMake build tree), then
// the bash harness output
let m;
try {
    m = require('composites_js');
} catch (e) {
    m = require('../../../build/composites_js.node');
}

let passed = 0;

function ok(message) {
    passed += 1;
    console.log(`  PASS: ${message}`);
}

function assert(condition, message) {
    if (!condition) throw new Error(message);
}

function rejects(fn) {
    try {
        fn();
    } catch (e) {
        assert(e instanceof TypeError, `threw ${e.constructor.name}, not TypeError: ${e.message}`);
        return e.message;
    }
    throw new Error('the assignment or call was accepted');
}

function mentions(message, ...needles) {
    for (const needle of needles) {
        assert(message.includes(needle), `expected '${needle}' in: ${message}`);
    }
}

function point(x, y) {
    const p = new m.Point2();
    p.x = x;
    p.y = y;
    return p;
}

// A smart pointer to an abstract class: only null and undefined convert
const scene = new m.Scene();
scene.set_circle(2);
mentions(rejects(() => { scene.shape = 42; }), 'shape');
assert(scene.has_shape(), 'a refused assignment cleared the member');
mentions(rejects(() => { scene.shape = new m.Circle(1); }), 'shape', 'Circle');
assert(scene.has_shape(), 'a refused assignment cleared the member');
ok('a value that is not null is refused, and the member keeps its pointer');

scene.shape = null;
assert(!scene.has_shape(), 'null did not clear the member');
assert(scene.area_of(null) === -1 && scene.area_of(undefined) === -1, 'area_of(null)');
ok('null still clears it, and passes as a null argument');

mentions(rejects(() => scene.area_of(new m.Circle(1))), 'area_of', 'argument 1');
ok('an argument that cannot become the smart pointer is refused, not passed as null');

// std::array: exactly N elements, filled in place
const body = new m.Body();
assert(body.sum3([1, 2, 3]) === 6, 'sum3 received zeros');
body.pos = [1, 2, 3];
assert(body.pos_sum() === 6, 'pos was not filled');
assert(JSON.stringify(body.pos) === '[1,2,3]', 'pos read back wrong');
ok('a std::array member and parameter receive the elements they were given');

rejects(() => { body.pos = [1, 2]; });
rejects(() => { body.pos = [1, 2, 3, 4]; });
rejects(() => { body.pos = [1, 'two', 3]; });
assert(body.pos_sum() === 6, 'a refused assignment changed the member');
mentions(rejects(() => body.sum3([1, 2, 3, 4])), 'sum3', 'argument 1');
ok('the wrong length or a bad element is refused, and the member keeps its value');

// std::span: cannot be filled from an array, so it is refused
mentions(rejects(() => body.total([1, 2, 3])), 'total', 'argument 1');
ok('a std::span parameter is refused rather than computed over nothing');

// Element types the container converters could not see. A class element is
// read field by field, from a plain object or from a wrapper.
const c = new m.Collections();

c.points = [{ x: 1, y: 2 }, point(3, 4)];
assert(c.points_sum() === 10, 'vector<Point2>');
const points = c.points;
assert(points.length === 2 && points[1].x === 3 && points[1].y === 4, 'vector<Point2> read back');
c.points = points;
assert(c.points_sum() === 10, 'vector<Point2> did not round-trip its own wrappers');
rejects(() => { c.points = [new m.Mark()]; });
rejects(() => { c.shared = [new m.Mark()]; });
assert(c.points_sum() === 10, 'a refused assignment changed the member');
ok('std::vector of a bound class: from objects, from wrappers, and back; not from another class');

c.ends = [point(1, 1), { x: 2, y: 2 }];
assert(c.ends_sum() === 6, 'array<Point2, 2>');
rejects(() => { c.ends = [point(1, 1)]; });
assert(c.ends_sum() === 6, 'a refused assignment changed the member');
ok('std::array of a bound class');

c.maybe = [1, null, 3];
assert(c.maybe_sum() === 104, 'vector<optional<int>>');
c.fill_maybe();
assert(JSON.stringify(c.maybe) === '[1,null,3]', 'vector<optional<int>> read back');
ok('std::vector of std::optional');

c.shared = [{ x: 1, y: 2 }, point(3, 4)];
assert(c.shared_sum() === 10, 'vector<shared_ptr<Point2>>');
assert(c.shared[1].x === 3, 'vector<shared_ptr<Point2>> read back');
ok('std::vector of std::shared_ptr');

c.pairs = [[2, 1.5], [4, 0.25]];
assert(c.pairs_sum() === 4, 'vector<pair<int, double>>');
assert(JSON.stringify(c.pairs) === '[[2,1.5],[4,0.25]]', 'vector<pair> read back');
ok('std::vector of std::pair');

c.flags = [true, false, true];
assert(c.flag_count() === 2, 'vector<bool>');
assert(JSON.stringify(c.flags) === '[true,false,true]', 'vector<bool> read back');
ok('std::vector<bool>, whose elements iterate as a proxy under libc++');

c.frames = [[1, 2, 3], [4, 5, 6]];
assert(c.frames_sum() === 21, 'vector<array<double, 3>>');
rejects(() => { c.frames = [[1, 2]]; });
assert(c.frames_sum() === 21, 'a refused assignment changed the member');
ok('std::vector of std::array checks each element\'s length');

c.add_cell(1, 2);
const cells = c.cells;
assert(cells.length === 1 && cells[0].row === 1 && cells[0].col === 2, 'vector<Cell> read back');
mentions(rejects(() => { c.cells = []; }), 'cells');
assert(c.cell_count() === 1, 'a refused assignment changed the member');
ok('a vector of a class with no default constructor reads, and refuses assignment');

// A variant or an optional of a class needs every field
const choices = new m.Choices();
choices.pc = { r: 1, g: 2, b: 3 };
assert(choices.pc_index() === 1 && choices.pc_red() === 1, 'a Color object became a Point2');
choices.pc = { x: 1, y: 2 };
assert(choices.pc_index() === 0, 'a Point2 object did not select Point2');
ok('std::variant of two classes picks the one whose fields the object has');

rejects(() => { choices.maybe_point = {}; });
rejects(() => { choices.maybe_point = { x: 5 }; });
assert(!choices.has_point(), 'an object missing fields became a Point2');
choices.maybe_point = { x: 5, y: 6 };
assert(choices.has_point(), 'a complete object did not convert');
choices.maybe_point = point(1, 2);
assert(choices.has_point(), 'a wrapper did not convert');
ok('std::optional of a class refuses an object that is missing fields, and takes a wrapper');

const owner = new m.Owner();
owner.head = { value: 4, extra: 9 };
assert(owner.head_value() === 4 && owner.head_extra() === 9, 'optional<Node> from an object');
ok('a movable, non-copyable class inside a composite still converts from an object');

const maps = new m.Maps();
maps.layers = [{ a: 1, b: 2 }, { c: 3 }];
assert(maps.layers_total() === 6, 'vector<map<string, int>>');
assert(maps.layers[1].c === 3, 'vector<map> read back');
maps.items = [{ tags: { a: 1 } }, { tags: { b: 2, c: 3 } }];
assert(maps.items_total() === 6, 'vector of a struct holding a map');
ok('a map inside a vector, and inside a struct, converts from an object');

console.log('');
console.log(`All ${passed} JavaScript composite tests passed!`);
