var sys = require('sys');
var libdtrace = require('libdtrace');
var dtp = new libdtrace.Consumer();

dtp.strcompile('BEGIN { trace("hello world"); }');
dtp.go();

dtp.consume(function (probe, rec) {
	sys.puts(sys.inspect(probe));
	sys.puts(sys.inspect(rec));
});
