var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

dtp = new libdtrace.Consumer();

ver = dtp.version().split(' ')[2].split('.');

if (parseInt(ver[0]) == 1 && parseInt(ver[1]) <= 8) {
	sys.puts('llquantize() not present in version ' + dtp.version() +
	    '; not testing.');
	process.exit(0);
}

prog = 'BEGIN\n{\n';

for (i = 0; i < 101; i++)
	prog += '\t@ = llquantize(' + i + ', 10, 0, 1, 20);\n';

prog += '}\n';

dtp.strcompile(prog);

dtp.go();

dtp.aggwalk(function (varid,key, val) {
	var expected = [
	    [ [ 0, 0 ], 1 ],
	    [ [ 1, 1 ], 1 ],
	    [ [ 2, 2 ], 1 ],
	    [ [ 3, 3 ], 1 ],
	    [ [ 4, 4 ], 1 ],
	    [ [ 5, 5 ], 1 ],
	    [ [ 6, 6 ], 1 ],
	    [ [ 7, 7 ], 1 ],
	    [ [ 8, 8 ], 1 ],
	    [ [ 9, 9 ], 1 ],
	    [ [ 10, 14 ], 5 ],
	    [ [ 15, 19 ], 5 ],
	    [ [ 20, 24 ], 5 ],
	    [ [ 25, 29 ], 5 ],
	    [ [ 30, 34 ], 5 ],
	    [ [ 35, 39 ], 5 ],
	    [ [ 40, 44 ], 5 ],
	    [ [ 45, 49 ], 5 ],
	    [ [ 50, 54 ], 5 ],
	    [ [ 55, 59 ], 5 ],
	    [ [ 60, 64 ], 5 ],
	    [ [ 65, 69 ], 5 ],
	    [ [ 70, 74 ], 5 ],
	    [ [ 75, 79 ], 5 ],
	    [ [ 80, 84 ], 5 ],
	    [ [ 85, 89 ], 5 ],
	    [ [ 90, 94 ], 5 ],
	    [ [ 95, 99 ], 5 ],
	    [ [ 100, 9223372036854776000 ], 1 ],
	];

	assert.deepEqual(val, expected);
});

dtp = new libdtrace.Consumer();

prog = 'BEGIN\n{\n';

for (i = 0; i < 10100; i += 50)
	prog += '\t@ = llquantize(' + i + ', 10, 2, 3, 10);\n';

prog += '}\n';

dtp.strcompile(prog);

dtp.go();

dtp.aggwalk(function (varid, key, val) {
	var expected =
	    [ [ [ 0, 99 ], 2 ],
	    [ [ 100, 199 ], 2 ],
	    [ [ 200, 299 ], 2 ],
	    [ [ 300, 399 ], 2 ],
	    [ [ 400, 499 ], 2 ],
	    [ [ 500, 599 ], 2 ],
	    [ [ 600, 699 ], 2 ],
	    [ [ 700, 799 ], 2 ],
	    [ [ 800, 899 ], 2 ],
	    [ [ 900, 999 ], 2 ],
	    [ [ 1000, 1999 ], 20 ],
	    [ [ 2000, 2999 ], 20 ],
	    [ [ 3000, 3999 ], 20 ],
	    [ [ 4000, 4999 ], 20 ],
	    [ [ 5000, 5999 ], 20 ],
	    [ [ 6000, 6999 ], 20 ],
	    [ [ 7000, 7999 ], 20 ],
	    [ [ 8000, 8999 ], 20 ],
	    [ [ 9000, 9999 ], 20 ],
	    [ [ 10000, 9223372036854776000 ], 2 ]
	];

	assert.deepEqual(val, expected);
});

