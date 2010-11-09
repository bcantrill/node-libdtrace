var util = require('util');
var libdtrace = require('libdtrace');
var i;

dtp = new libdtrace.Consumer();

prog =
	'sched:::on-cpu\n' +
	'{\n' + 
	'	self->on = timestamp;\n' +
	'}\n' +
	'\n' +
	'sched:::off-cpu\n' +
	'/self->on/\n' +
	'{\n' + 
	'	@ = lquantize((timestamp - self->on) / 1000, \n' +
	'	    0, 10000, 100);\n' +
	'}\n' +
	'\n';

util.puts(prog);

dtp.setopt('aggrate', '10ms');
dtp.strcompile(prog);
dtp.go();
data = [];

var start = new Date();

setInterval(function () {

	var delta = (new Date()).valueOf() - start.valueOf();
	
	if (delta >= 5000)
		process.exit(0);

	dtp.aggwalk(function (varid, key, val) {
		data.push(val);

		if (data.length >= 50)
			return;
			
		for (i = data.length - 1; i >= 0 && i >= data.length - 5; i--)
			util.puts(util.inspect(data[i], false, null));
	});
}, 10);

