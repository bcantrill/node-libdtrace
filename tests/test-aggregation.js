var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

dtp = new libdtrace.Consumer();
dtp.strcompile('BEGIN { @["foo", "bar", 9904, 61707] = count(); }');

dtp.go();

dtp.aggwalk(function (varid, key, val) {
	assert.equal(varid, 1);
	assert.ok(key instanceof Array, 'expected key to be an array');
	assert.equal(key.length, 4);
	assert.equal(key[0], "foo");
	assert.equal(key[1], "bar");
	assert.equal(key[2], 9904);
	assert.equal(key[3], 61707);
	assert.equal(val, 1);
});

dtp.aggwalk(function (varid, key, val) {
	assert.ok(false, 'did not expect to find aggregation contents');
});

dtp = new libdtrace.Consumer();

var lq = function (val)
{
	return (val + ', 3, 7, 3')
};

aggacts = {
	max: { args: [ '10', '20' ], expected: 20 },
	min: { args: [ '10', '20' ], expected: 10 },
	count: { args: [ '', '' ], expected: 2 },
	sum: { args: [ '10', '20' ], expected: 30 },
	avg: { args: [ '30', '1' ], expected: 15.5 },
	quantize: { args: [ '2', '4', '5', '8' ], expected: [
	    [ [ 2, 3 ], 1 ],
	    [ [ 4, 7 ], 2 ],
	    [ [ 8, 15 ], 1 ]
	] },
	lquantize: { args: [ lq(2), lq(4), lq(5), lq(8) ], expected: [
	    [ [ dtp.aggmin(), 2 ], 1 ],
	    [ [ 3, 5 ], 2 ],
	    [ [ 6, dtp.aggmax() ], 1 ]
	] }
};

varids = [ '' ];
prog = 'BEGIN\n{\n';

for (act in aggacts) {
	varids.push(act);

	for (i = 0; i < aggacts[act].args.length; i++) {
		prog += '\t@agg' + act + ' = ' + act + '(' +
		     aggacts[act].args[i] + ');\n';
	}
}

prog += '}\n';

dtp.strcompile(prog);

dtp.go();

dtp.aggwalk(function (varid, key, val) {
	assert.ok(varids[varid], 'invalid variable ID ' + varid);
	assert.ok(aggacts[varids[varid]], 'unknown variable ID ' + varid);

	act = aggacts[varids[varid]];
	assert.deepEqual(act.expected, val);
});

