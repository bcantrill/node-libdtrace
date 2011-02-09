var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');
var fs = require('fs');

var dtp = new libdtrace.Consumer();

var ver = dtp.version();

elems = ver.split(' ');
assert.equal(elems.length, 3);
assert.equal(elems[0], 'Sun');
assert.equal(elems[1], 'D');

vernum = elems[2].split('.');

assert.ok(vernum.length >= 2,
    'expected dot-delimited version; found "' + elems[2] + '"');

/*
 * Given the constraints around changing the major number for DTrace, it's
 * reasonable to assume that if does indeed happen, other elements of
 * node-libdtrace have broken as well.
 */
assert.equal(vernum[0], '1', 'According to dt_open.c: "The major number ' +
    'should be incremented when a fundamental change has been made that ' +
    'would affect all consumers, and would reflect sweeping changes to ' +
    'DTrace or the D language."  Given that the major number seems to have ' +
    'been bumped to ' + vernum[0] + ', it appears that this rapture is upon ' +
    'us!  Committing ritual suicide accordingly...');

assert.ok(parseInt(vernum[1], 10) >= 5);

