var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

dtp = new libdtrace.Consumer();

prog = 'BEGIN\n{\n';

for (i = -32; i < 32; i++)
	prog += '\t@ = quantize(' + i + ');\n';

prog += '}\n';

sys.puts(prog);

dtp.strcompile(prog);

dtp.go();

dtp.aggwalk(function (varid, key, val) {
	var expected = [
		[ [ -63, -32 ], 1 ],
		[ [ -31, -16 ], 16 ],
		[ [ -15, -8 ], 8 ],
		[ [ -7, -4 ], 4 ],
		[ [ -3, -2 ], 2 ],
		[ [ -1, -1 ], 1 ],
		[ [ 0, 0 ], 1 ],
		[ [ 1, 1 ], 1 ],
		[ [ 2, 3 ], 2 ],
		[ [ 4, 7 ], 4 ],
		[ [ 8, 15 ], 8 ],
		[ [ 16, 31 ], 16 ],
	];

	assert.equal(varid, 1);
	assert.ok(key instanceof Array, 'expected key to be an array');
	assert.equal(key.length, 0);
	assert.ok(val instanceof Array, 'expected val to be an array');
	assert.deepEqual(expected, val);
	sys.puts(sys.inspect(val));
});

delete dtp;

