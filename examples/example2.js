var sys = require('sys');
var libdtrace = require('libdtrace');
var dtp = new libdtrace.Consumer();

var prog = 'syscall:::entry { @[execname] = count(); }'

dtp.strcompile(prog);
dtp.go();

var syscalls = {};
var keys = [];

var pad = function (val, len)
{
	var rval = '', i, str = val + '';

	for (i = 0; i < Math.abs(len) - str.length; i++)
		rval += ' ';

	rval = len < 0 ? str + rval : rval + str;

	return (rval);
};

setInterval(function () {
	var i;

	sys.puts(pad('EXECNAME', -40) + pad('COUNT', -10));

	dtp.aggwalk(function (id, key, val) {
		if (!syscalls.hasOwnProperty(key[0]))
			keys.push(key[0]);

		syscalls[key[0]] = val;
	});

	keys.sort();

	for (i = 0; i < keys.length; i++) {
		sys.puts(pad(keys[i], -40) + pad(syscalls[keys[i]], -10));
		syscalls[keys[i]] = 0;
	}

	sys.puts('');
}, 1000);
