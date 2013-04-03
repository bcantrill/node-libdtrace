
node-libdtrace
==============

Overview
--------

node-libdtrace is a Node.js addon that interfaces to libdtrace, allowing
node programs to control DTrace enablings.

Status
------

The primary objective is not to create a `dtrace(1M)` alternative in node, but
rather to allow node programs to create and control programmatically useful
DTrace enablings.  That is, the goal is software-software interaction, and as
such, DTrace actions related to controlling output (e.g., `printf()`,
`printa()`) are not supported.  Error handling is, for the moment, weak.

Platforms
---------

This should work on any platform that supports DTrace, and is known to work on
Mac OS X (tested on 10.7.5) and illumos (tested on
SmartOS).

Installation
------------

As an addon, node-libdtrace is installed in the usual way:

      % npm install libdtrace

API
---

### `new libdtrace.Consumer()`

Create a new libdtrace consumer, which will correspond to a new `libdtrace`
state.  If DTrace cannot be initalized for any reason, this will throw an
exception with the `message` member set to the more detailed reason from
libdtrace.  Note that one particularly common failure mode is attempting to
initialize DTrace without the necessary level of privilege; in this case, for
example, the `message` member will be:

      DTrace requires additional privileges

(The specifics of this particular message should obviously not be 
programmatically depended upon.)  If encountering this error, you will
need to be a user that has DTrace privileges.

### `consumer.strcompile(str)`

Compile the specified `str` as a D program.  This is required before
any call to `consumer.go()`.

### `consumer.go()`

Instruments the system using the specified enabling.  Before `consumer.go()`
is called, the specified D program has been compiled but not executed; once
`consumer.go()` is called, no further D compilation is possible.

### `consumer.setopt(option, value)`

Sets the specified `option` (a string) to `value` (an integer, boolean,
string, or string representation of an integer or boolean, as denoted by
the option being set).

### `consumer.consume(function func (probe, rec) {})`

Consume any DTrace data traced to the principal buffer since the last call to
`consumer.consume()` (or the call to `consumer.go()` if `consumer.consume()`
has not been called).  For each trace record, `func` will be called and
passed two arguments:

* `probe` is an object that specifies the probe that corresponds to the
   trace record in terms of the probe tuple: provider, module, function
   and name.

* `rec` is an object that has a single member, `data`, that corresponds to
   the datum within the trace record.  If the trace record has been entirely
   consumed, `rec` will be `undefined`.

In terms of implementation, a call to `consumer.consume()` will result in a
call to `dtrace_status()` and a principal buffer switch.  Note that if the
rate of consumption exceeds the specified `switchrate` (set via either
`#pragma D option switchrate` or `consumer.setopt()`), this will result in no
new data processing.

### `consumer.aggwalk(function func (varid, key, value) {})`

Snapshot and iterate over all aggregation data accumulated since the
last call to `consumer.aggwalk()` (or the call to `consumer.go()` if
`consumer.aggwalk()` has not been called).  For each aggregate record,
`func` will be called and passed three arguments:

* `varid` is the identifier of the aggregation variable.  These IDs are
  assigned in program order, starting with 1.

* `key` is an array of keys that, taken with the variable identifier,
  uniquely specifies the aggregation record.

* `value` is the value of the aggregation record, the meaning of which
  depends on the aggregating action:

  * For `count()`, `sum()`, `max()` and `min()`, the value is the
    integer value of the aggregation action

  * For `avg()`, the value is the numeric value of the aggregating action

  * For `quantize()` and `lquantize()`, the value is an array of 2-tuples
    denoting ranges and value:  each element consists of a two element array
    denoting the range (minimum followed by maximum, inclusive) and the
    value for that range.  

Upon return from `consumer.aggwalk()`, the aggregation data for the specified
variable and key(s) is removed.

Note that the rate of `consumer.aggwalk()` actually consumes the aggregation
buffer is clamed by the `aggrate` option; if `consumer.aggwalk()` is called
more frequently than the specified rate, `consumer.aggwalk()` will not
induce any additional data processing.

`consumer.aggwalk()` does not iterate over aggregation data in any guaranteed
order, and may interleave aggregation variables and/or keys.

### `consumer.version()`

Returns the version string, as returned from `dtrace -V`.

Examples
--------

### Hello world

The obligatory "hello world":

      var sys = require('sys');
      var libdtrace = require('libdtrace');
      var dtp = new libdtrace.Consumer();
        
      var prog = 'BEGIN { trace("hello world"); }';
        
      dtp.strcompile(prog);
      dtp.go();
        
      dtp.consume(function (probe, rec) {
              if (rec)
                      sys.puts(rec.data);
      });

### Using aggregations

A slightly more sophisticated example showing system calls aggregated and
sorted by executable name:

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
