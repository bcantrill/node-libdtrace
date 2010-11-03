var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

dtp = new libdtrace.Consumer();

prog = 'BEGIN\n{\n';

for (i = -5; i < 15; i++)
	prog += '\t@ = lquantize(' + i + ', 0, 10, 2);\n';

prog += '}\n';

sys.puts(prog);

dtp.strcompile(prog);

dtp.go();

dtp.aggwalk(function (varid, key, val) {
	sys.puts(sys.inspect(val));
});

dtp = new libdtrace.Consumer();

prog = 'BEGIN\n{\n';

for (i = -100; i < 100; i++)
	prog += '\t@ = lquantize(' + i + ', -200, 200, 10);\n';

prog += '}\n';

dtp.strcompile(prog);

dtp.go();

dtp.aggwalk(function (varid, key, val) {
	sys.puts(sys.inspect(val));
});

delete dtp;

