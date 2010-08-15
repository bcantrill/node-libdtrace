var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

dtp = new libdtrace.Consumer();
assert.throws(function () { dtp.strcompile(); });
assert.throws(function () { dtp.strcompile(61707); });
assert.throws(function () { dtp.strcompile('this is not D'); });
assert.throws(function () { dtp.strcompile('bogus-probe { trace(0); }') });
dtp.strcompile('BEGIN { trace(9904); }');

assert.throws(function () { dtp.setopt() });
assert.throws(function () { dtp.setopt('bogusoption') });
assert.throws(function () { dtp.setopt('bufpolicy') });
assert.throws(function () { dtp.setopt('bufpolicy', 100) });

dtp.setopt('bufpolicy', 'ring');
dtp.setopt('bufpolicy', 'switch');

seen = false;
lastrec = false;

dtp.go();

dtp.consume(function testbasic (probe, rec) {
	assert.equal(probe.provider, 'dtrace');
	assert.equal(probe.module, '');
	assert.equal(probe.function, '');
	assert.equal(probe.name, 'BEGIN');

	if (!rec) {
		assert.ok(seen);
		lastrec = true;
	} else {
		seen = true;
		assert.equal(rec.data, 9904);
	}
});

assert.ok(seen, 'did not consume expected record');
assert.ok(lastrec, 'did not see delineator between EPIDs');
assert.throws(function () { dtp.go(); });
assert.throws(function () { dtp.strcompile('BEGIN { trace(0); }') });

dtp.stop();

/*
 * Now test that END clauses work properly.
 */
dtp = new libdtrace.Consumer();
dtp.strcompile('END { trace(61707); }');

dtp.go();

seen = false;

dtp.consume(function testend (probe, rec) {
	assert.ok(false);
});

dtp.stop();

dtp.consume(function testend_consume (probe, rec) {
	assert.equal(probe.provider, 'dtrace');
	assert.equal(probe.module, '');
	assert.equal(probe.function, '');
	assert.equal(probe.name, 'END');

	if (!rec)
		return;

	assert.equal(rec.data, 61707);
});

dtp = new libdtrace.Consumer();
dtp.strcompile('tick-1sec { trace(i++); }');
dtp.go();
secs = 0;
val = 0;

id = setInterval(function testtick () {
	assert.ok(secs++ < 10, 'failed to terminate (val is ' + val + ')');

	dtp.consume(function testtick_consume (probe, rec) {
		if (!rec)
			return;

		if ((val = rec.data) > 5)
			clearInterval(id);

		sys.puts(sys.inspect(rec));
	});
}, 1000);

