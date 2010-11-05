var sys = require('sys');
var libdtrace = require('libdtrace');
var dtp = new libdtrace.Consumer();

var prog = 'BEGIN { trace("hello world"); }\n'

dtp.strcompile(prog);
dtp.go();

dtp.consume(function (probe, rec) {
	if (rec)
		sys.puts(rec.data);
});
