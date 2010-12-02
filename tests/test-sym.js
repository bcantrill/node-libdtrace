var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');
var fs = require('fs');

var test = function (action, valid, work)
{
	/*
	 * First, verify that the action can be compiled and consumed.
	 */
	var dtp = new libdtrace.Consumer();
	sys.puts('>>>> basic compile for ' + action + '()');

	dtp.strcompile('BEGIN { ' + action + '(0); }');

	dtp.go();

	var found = false, start, now;

	if (!work)
		work = function () {};

	dtp.consume(function testbasic (probe, rec) {
		assert.equal(probe.provider, 'dtrace');
		assert.equal(probe.module, '');
		assert.equal(probe.function, '');
		assert.equal(probe.name, 'BEGIN');

		if (!rec || !rec.hasOwnProperty('data'))
			return;

		found = true;
		sys.puts('  ' + sys.inspect(rec));
	});

	assert.ok(found, 'did not find valid data record');

	var argument = (action.indexOf('u') == 0 ? 'arg1' : 'arg0');

	/*
	 * Now verify the straight consumption.
	 */
	sys.puts('>>>> consumption for ' + action +
	    '() (using ' + argument + ' on profile probe)');

	dtp = new libdtrace.Consumer();
	dtp.strcompile('profile-97hz /' + argument + ' != 0/ ' +
	    '{ ' + action + '(' + argument + ') }');

	dtp.go();

	start = (new Date()).valueOf();

	do {
		now = (new Date()).valueOf();
		work();
	} while (now - start < 2000);

	found = false;

	dtp.consume(function testbasic (probe, rec) {
		if (!rec)
			return;

		if (valid(rec.data))
			found = true;

		sys.puts('  ' + sys.inspect(rec));
	});

	dtp.stop();
	assert.ok(found, 'did not find valid record in principal buffer');

	sys.puts('>>>> aggregation on ' + action +
	    '() (using ' + argument + ' on profile probe)');

	dtp = new libdtrace.Consumer();
	dtp.strcompile('profile-97hz /' + argument + ' != 0/ ' +
	    '{ @[' + action + '(' + argument + ')] = count() }');

	dtp.setopt('aggrate', '10ms');
	dtp.go();

	var start = new Date().valueOf();

	do {
		now = (new Date()).valueOf();
		work();
	} while (now - start < 2000);

	found = false;

	dtp.aggwalk(function (varid, key, val) {
		if (valid(key[0]))
			found = true;

		sys.puts('  ' + sys.inspect(key) + ': ' + sys.inspect(val));
	});
};

var validsym = function (str)
{
	assert.ok(str.indexOf('+0x') == -1,
	    'expected symbol without offset; found "' + str + '"');

	return (str.indexOf('`') != -1 && str.indexOf('+') == -1);
}

var validaddr = function (str)
{
	assert.ok(str.indexOf('0x') == 0 || str.indexOf('`') != -1,
	    'expected address; found "' + str + '"');

	return (str.indexOf('`') != -1);
}

var kernelwork = function ()
{
	fs.unlink('/tmp/does/not/exist');
}

var validkmod = function (str)
{
	if (str == 'unix' || 'genunix')			/* Solaris */
		return (true);

	if (str == 'mach_kernel')			/* OS X */
		return (true);

	return (false);
};

var validmod = function (str)
{
	if (str.indexOf('.so.') != -1)			/* Solaris */
		return (true);

	if (str.indexOf('dylib') != -1)			/* OS X */
		return (true);
};

test('sym', validsym, kernelwork);
test('func', validsym, kernelwork);
test('mod', validkmod, kernelwork);

test('uaddr', validaddr);
test('usym', validsym);
test('umod', validmod);

/*
 * Note that this test may induce a seg fault in node on at least some variants
 * of Mac OS X.  For more details, see the post to dtrace-discuss with the
 * subject "Bug in dtrace_uaddr2str() on OS X?" (Dec. 1, 2010).  (As of this
 * writing, the bug had been root-caused within Apple, and a fix forthcoming.)
 */
test('ufunc', validsym);

