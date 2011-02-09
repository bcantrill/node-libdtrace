var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

dtp = new libdtrace.Consumer();
dtp.strcompile('BEGIN { printf("{ foo: %d", 123); printf(", bar: %d", 456); }'); 

dtp.go();

dtp.consume(function testbasic (probe, rec) {
	assert.equal(probe.provider, 'dtrace');
	assert.equal(probe.module, '');
	assert.equal(probe.function, '');
	assert.equal(probe.name, 'BEGIN');

	sys.puts(sys.inspect(probe));
	sys.puts(sys.inspect(rec));
});

dtp.stop();

