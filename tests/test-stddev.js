var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

/*
 * Don't get your hopes up -- this test is asserting that stddev() is
 * properly not supported.
 */
dtp = new libdtrace.Consumer();

dtp.strcompile('BEGIN { @ = stddev(0) }');

dtp.go();

assert.throws(function () { dtp.strcompile('BEGIN { @ = stddev(0) }') });

