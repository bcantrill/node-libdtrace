var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

dtp = new libdtrace.Consumer();

assert.throws(function () { dtp.setopt() });
assert.throws(function () { dtp.setopt('bogusoption') });
assert.throws(function () { dtp.setopt('bufpolicy') });
assert.throws(function () { dtp.setopt('bufpolicy', 100) });
dtp.setopt('quiet', true);
assert.throws(function () { dtp.setopt('bufpolicy', true); });
assert.throws(function () { dtp.setopt('dynvarsize', true); });
assert.throws(function () { dtp.setopt('dynvarsize', 1.23); });
dtp.setopt('quiet', 1024);
assert.throws(function () { dtp.setopt('quiet', { foo: 1024 }); });
assert.throws(function () { dtp.setopt('quiet', [ false ]); });
assert.throws(function () { dtp.setopt('quiet', 1.024); });
assert.throws(function () { dtp.setopt('quiet', 1.024); });
assert.throws(function () { dtp.setopt('quiet', new Date); });
dtp.setopt('quiet');
dtp.setopt('quiet', true);
dtp.setopt('quiet', false);
dtp.setopt('quiet', '1');
dtp.setopt('quiet', '0');


