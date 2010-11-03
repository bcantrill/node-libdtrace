var sys = require('sys');
var libdtrace = require('libdtrace');
var assert = require('assert');

var tests = [
    'tick-1000hz { @a = quantize(0); @b = quantize(0) }',
    'tick-1000hz { @a = quantize(rand()); }',
    'tick-1000hz { @a = lquantize(0, 0, 100, 1); }',
    'tick-1000hz { @a = lquantize(0, 0, 100, 1); @b = lquantize(10, -10, 10); }',
];

var pad = function (val, len)
{
	var rval = '', i;
	var str = val + '';

	for (i = 0; i < len - str.length; i++)
		rval += ' ';

	rval += str;

	return (rval);
};

var seconds = -1;
var end = 30;
var dtp = undefined;
var dumped = -1;

setInterval(function () {
	if (!dtp) {
		if (!tests.length)
			process.exit(0);

		test = tests.shift();
		start = new Date().valueOf();

		dtp = new libdtrace.Consumer();

		sys.puts(seconds != -1 ? '\n' : '');
		sys.puts('Testing heap consumption of:\n  ' + test + '\n');
		sys.puts(pad('SECONDS', 9) + pad('HEAP-USED', 20) +
		    pad('HEAP-TOTAL', 20));
		dtp.strcompile(test);
		dtp.setopt('aggrate', '5000hz');
		dtp.go();
	}

	dtp.aggwalk(function (varid, key, val) {});

	seconds = Math.floor((new Date().valueOf() - start) / 1000);

	if (seconds != dumped && seconds % 5 == 0) {
		dumped = seconds;
		var usage = process.memoryUsage();

		sys.puts(pad(seconds, 9) + pad(usage.heapUsed, 20) +
		    pad(usage.heapTotal, 20));
	}

	if (seconds >= end) {
		delete dtp;
		dtp = undefined;
	}	
}, 10);
