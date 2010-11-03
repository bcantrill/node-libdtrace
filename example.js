var sys = require('sys');
var libdtrace = require('libdtrace');
var dtp = new libdtrace.Consumer();

var prog = 'BEGIN { trace("hello world"); }\n'
prog += 'BEGIN { @["hello"] = sum(1); @["world"] = sum(2); }'

dtp.strcompile(prog);

dtp.go();

dtp.consume(function (probe, rec) {
	sys.puts(sys.inspect(probe));
	sys.puts(sys.inspect(rec));
});

dtp.aggwalk(function (varid, key, value) {
	sys.puts(sys.inspect(key));
	sys.puts(sys.inspect(value));
});
