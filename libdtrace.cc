/*
 * For whatever reason, g++ on Solaris defines _XOPEN_SOURCE -- which in
 * turn will prevent us from pulling in our desired definition for boolean_t.
 * We don't need it, so explicitly undefine it.
 */
#ifdef _XOPEN_SOURCE
#undef _XOPEN_SOURCE
#endif

#include <v8.h>
#include <node.h>
#include <string.h>
#include <unistd.h>
#include <node_object_wrap.h>
#include <errno.h>
#include <string>
#include <vector>

/*
 * Sadly, libelf refuses to compile if _FILE_OFFSET_BITS has been manually
 * jacked to 64 on a 32-bit compile.  In this case, we just manually set it
 * back to 32.
 */
#if defined(_ILP32) && (_FILE_OFFSET_BITS != 32)
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#endif

#include <dtrace.h>

/*
 * This is a tad unsightly:  if we didn't find the definition of the
 * llquantize() aggregating action, we're going to redefine it here (along
 * with its support cast of macros).  This allows node-libdtrace to operate
 * on a machine that has llquantize(), even if it was compiled on a machine
 * without the support.
 */
#ifndef DTRACEAGG_LLQUANTIZE

#define	DTRACEAGG_LLQUANTIZE			(DTRACEACT_AGGREGATION + 9)

#define	DTRACE_LLQUANTIZE_FACTORSHIFT		48
#define	DTRACE_LLQUANTIZE_FACTORMASK		((uint64_t)UINT16_MAX << 48)
#define	DTRACE_LLQUANTIZE_LOWSHIFT		32
#define	DTRACE_LLQUANTIZE_LOWMASK		((uint64_t)UINT16_MAX << 32)
#define	DTRACE_LLQUANTIZE_HIGHSHIFT		16
#define	DTRACE_LLQUANTIZE_HIGHMASK		((uint64_t)UINT16_MAX << 16)
#define	DTRACE_LLQUANTIZE_NSTEPSHIFT		0
#define	DTRACE_LLQUANTIZE_NSTEPMASK		UINT16_MAX

#define DTRACE_LLQUANTIZE_FACTOR(x)             \
	(uint16_t)(((x) & DTRACE_LLQUANTIZE_FACTORMASK) >> \
	DTRACE_LLQUANTIZE_FACTORSHIFT)

#define DTRACE_LLQUANTIZE_LOW(x)                \
        (uint16_t)(((x) & DTRACE_LLQUANTIZE_LOWMASK) >> \
        DTRACE_LLQUANTIZE_LOWSHIFT)

#define DTRACE_LLQUANTIZE_HIGH(x)               \
        (uint16_t)(((x) & DTRACE_LLQUANTIZE_HIGHMASK) >> \
        DTRACE_LLQUANTIZE_HIGHSHIFT)

#define DTRACE_LLQUANTIZE_NSTEP(x)              \
        (uint16_t)(((x) & DTRACE_LLQUANTIZE_NSTEPMASK) >> \
        DTRACE_LLQUANTIZE_NSTEPSHIFT)
#endif

using namespace v8;
using std::string;
using std::vector;

class DTraceConsumer : node::ObjectWrap {
public:
	static void Initialize(Handle<Object> target);

protected:
	DTraceConsumer();
	~DTraceConsumer();

	Handle<Value> error(const char *fmt, ...);
	Handle<Value> badarg(const char *msg);
	boolean_t valid(const dtrace_recdesc_t *);
	const char *action(const dtrace_recdesc_t *, char *, int);
	Local<Value> record(const dtrace_recdesc_t *, caddr_t);
	Local<Object> probedesc(const dtrace_probedesc_t *);

	Local<Array> *ranges_cached(dtrace_aggvarid_t);
	Local<Array> *ranges_cache(dtrace_aggvarid_t, Local<Array> *);
	Local<Array> *ranges_quantize(dtrace_aggvarid_t);
	Local<Array> *ranges_lquantize(dtrace_aggvarid_t, uint64_t);
	Local<Array> *ranges_llquantize(dtrace_aggvarid_t, uint64_t, int);

	static int consume(const dtrace_probedata_t *data,
	    const dtrace_recdesc_t *rec, void *arg);
	static int aggwalk(const dtrace_aggdata_t *agg, void *arg);
	static int bufhandler(const dtrace_bufdata_t *bufdata, void *arg);

	static Handle<Value> New(const Arguments& args);
	static Handle<Value> Consume(const Arguments& args);
	static Handle<Value> Aggwalk(const Arguments& args);
	static Handle<Value> Aggmin(const Arguments& args);
	static Handle<Value> Aggmax(const Arguments& args);
	static Handle<Value> Strcompile(const Arguments& args);
	static Handle<Value> Setopt(const Arguments& args);
	static Handle<Value> Go(const Arguments& args);
	static Handle<Value> Stop(const Arguments& args);
	static Handle<Value> Version(const Arguments& args);

private:
	dtrace_hdl_t *dtc_handle;
	static Persistent<FunctionTemplate> dtc_templ;
	const Arguments *dtc_args;
	Local<Function> dtc_callback;
	Handle<Value> dtc_error;
	Local<Array> *dtc_ranges;
	dtrace_aggvarid_t dtc_ranges_varid;
};

Persistent<FunctionTemplate> DTraceConsumer::dtc_templ;

DTraceConsumer::DTraceConsumer() : node::ObjectWrap()
{
	int err;
	dtrace_hdl_t *dtp;

	if ((dtc_handle = dtp = dtrace_open(DTRACE_VERSION, 0, &err)) == NULL)
		throw (dtrace_errmsg(NULL, err));

	/*
	 * Set our buffer size and aggregation buffer size to the de facto
	 * standard of 4M.
	 */
	(void) dtrace_setopt(dtp, "bufsize", "4m");
	(void) dtrace_setopt(dtp, "aggsize", "4m");

	if (dtrace_handle_buffered(dtp, DTraceConsumer::bufhandler, this) == -1)
		throw (dtrace_errmsg(dtp, dtrace_errno(dtp)));

	dtc_ranges = NULL;
};

DTraceConsumer::~DTraceConsumer()
{
	if (dtc_ranges != NULL)
		delete [] dtc_ranges;

	dtrace_close(dtc_handle);
}

void
DTraceConsumer::Initialize(Handle<Object> target)
{
	HandleScope scope;
	Local<FunctionTemplate> dtc =
	    FunctionTemplate::New(DTraceConsumer::New);

	dtc_templ = Persistent<FunctionTemplate>::New(dtc);
	dtc_templ->InstanceTemplate()->SetInternalFieldCount(1);
	dtc_templ->SetClassName(String::NewSymbol("Consumer"));

	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "strcompile",
	    DTraceConsumer::Strcompile);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "setopt", DTraceConsumer::Setopt);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "go", DTraceConsumer::Go);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "consume",
	    DTraceConsumer::Consume);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "aggwalk",
	    DTraceConsumer::Aggwalk);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "aggmin", DTraceConsumer::Aggmin);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "aggmax", DTraceConsumer::Aggmax);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "stop", DTraceConsumer::Stop);
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "version",
	    DTraceConsumer::Version);

	target->Set(String::NewSymbol("Consumer"), dtc_templ->GetFunction());
}

Handle<Value>
DTraceConsumer::New(const Arguments& args)
{
	HandleScope scope;
	DTraceConsumer *dtc;

	try {
		dtc = new DTraceConsumer();
	} catch (char const *msg) {
		return (ThrowException(Exception::Error(String::New(msg))));
	}

	dtc->Wrap(args.Holder());

	return (args.This());
}

const char *
DTraceConsumer::action(const dtrace_recdesc_t *rec, char *buf, int size)
{
	static struct {
		dtrace_actkind_t action;
		const char *name;
	} act[] = {
		{ DTRACEACT_NONE,	"<none>" },
		{ DTRACEACT_DIFEXPR,	"<DIF expression>" },
		{ DTRACEACT_EXIT,	"exit()" },
		{ DTRACEACT_PRINTF,	"printf()" },
		{ DTRACEACT_PRINTA,	"printa()" },
		{ DTRACEACT_LIBACT,	"<library action>" },
		{ DTRACEACT_USTACK,	"ustack()" },
		{ DTRACEACT_JSTACK,	"jstack()" },
		{ DTRACEACT_USYM,	"usym()" },
		{ DTRACEACT_UMOD,	"umod()" },
		{ DTRACEACT_UADDR,	"uaddr()" },
		{ DTRACEACT_STOP,	"stop()" },
		{ DTRACEACT_RAISE,	"raise()" },
		{ DTRACEACT_SYSTEM,	"system()" },
		{ DTRACEACT_FREOPEN,	"freopen()" },
		{ DTRACEACT_STACK,	"stack()" },
		{ DTRACEACT_SYM,	"sym()" },
		{ DTRACEACT_MOD,	"mod()" },
		{ DTRACEAGG_COUNT,	"count()" },
		{ DTRACEAGG_MIN,	"min()" },
		{ DTRACEAGG_MAX,	"max()" },
		{ DTRACEAGG_AVG,	"avg()" },
		{ DTRACEAGG_SUM,	"sum()" },
		{ DTRACEAGG_STDDEV,	"stddev()" },
		{ DTRACEAGG_QUANTIZE,	"quantize()" },
		{ DTRACEAGG_LQUANTIZE,	"lquantize()" },
		{ DTRACEAGG_LLQUANTIZE,	"llquantize()" },
		{ DTRACEACT_NONE,	NULL },
	};

	dtrace_actkind_t action = rec->dtrd_action;
	int i;

	for (i = 0; act[i].name != NULL; i++) {
		if (act[i].action == action)
			return (act[i].name);
	}

	(void) snprintf(buf, size, "<unknown action 0x%x>", action);

	return (buf);
}

Handle<Value>
DTraceConsumer::error(const char *fmt, ...)
{
	char buf[1024], buf2[1024];
	char *err = buf;
	va_list ap;

	va_start(ap, fmt);
	(void) vsnprintf(buf, sizeof (buf), fmt, ap);

	if (buf[strlen(buf) - 1] != '\n') {
		/*
		 * If our error doesn't end in a new-line, we'll append the
		 * strerror of errno.
		 */
		(void) snprintf(err = buf2, sizeof (buf2),
		    "%s: %s", buf, strerror(errno));
	} else {
		buf[strlen(buf) - 1] = '\0';
	}

	return (ThrowException(Exception::Error(String::New(err))));
}

Handle<Value>
DTraceConsumer::badarg(const char *msg)
{
	return (ThrowException(Exception::TypeError(String::New(msg))));
}

boolean_t
DTraceConsumer::valid(const dtrace_recdesc_t *rec)
{
	dtrace_actkind_t action = rec->dtrd_action;

	switch (action) {
	case DTRACEACT_DIFEXPR:
	case DTRACEACT_SYM:
	case DTRACEACT_MOD:
	case DTRACEACT_USYM:
	case DTRACEACT_UMOD:
	case DTRACEACT_UADDR:
		return (B_TRUE);

	default:
		return (B_FALSE);
	}
}

Local<Value>
DTraceConsumer::record(const dtrace_recdesc_t *rec, caddr_t addr)
{
	switch (rec->dtrd_action) {
	case DTRACEACT_DIFEXPR:
		switch (rec->dtrd_size) {
		case sizeof (uint64_t):
			return (Number::New(*((int64_t *)addr)));

		case sizeof (uint32_t):
			return (Integer::New(*((int32_t *)addr)));

		case sizeof (uint16_t):
			return (Integer::New(*((uint16_t *)addr)));

		case sizeof (uint8_t):
			return (Integer::New(*((uint8_t *)addr)));

		default:
			return (String::New((const char *)addr));
		}

	case DTRACEACT_SYM:
	case DTRACEACT_MOD:
	case DTRACEACT_USYM:
	case DTRACEACT_UMOD:
	case DTRACEACT_UADDR:
		dtrace_hdl_t *dtp = dtc_handle;
		char buf[2048], *tick, *plus;

		buf[0] = '\0';

		if (DTRACEACT_CLASS(rec->dtrd_action) == DTRACEACT_KERNEL) {
			uint64_t pc = ((uint64_t *)addr)[0];
			dtrace_addr2str(dtp, pc, buf, sizeof (buf) - 1);
		} else {
			uint64_t pid = ((uint64_t *)addr)[0];
			uint64_t pc = ((uint64_t *)addr)[1];
			dtrace_uaddr2str(dtp, pid, pc, buf, sizeof (buf) - 1);
		}

		if (rec->dtrd_action == DTRACEACT_MOD ||
		    rec->dtrd_action == DTRACEACT_UMOD) {
			/*
			 * If we're looking for the module name, we'll
			 * return everything to the left of the left-most
			 * tick -- or "<undefined>" if there is none.
			 */
			if ((tick = strchr(buf, '`')) == NULL)
				return (String::New("<unknown>"));

			*tick = '\0';
		} else if (rec->dtrd_action == DTRACEACT_SYM ||
		    rec->dtrd_action == DTRACEACT_USYM) {
			/*
			 * If we're looking for the symbol name, we'll
			 * return everything to the left of the right-most
			 * plus sign (if there is one).
			 */
			if ((plus = strrchr(buf, '+')) != NULL)
				*plus = '\0';
		}

		return (String::New(buf));
	}

	assert(B_FALSE);
	return (Integer::New(-1));
}

Handle<Value>
DTraceConsumer::Strcompile(const Arguments& args)
{
	DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(args.Holder());
	dtrace_hdl_t *dtp = dtc->dtc_handle;
	dtrace_prog_t *dp;
	dtrace_proginfo_t info;

	if (args.Length() < 1 || !args[0]->IsString())
		return (dtc->badarg("expected program"));

	String::Utf8Value program(args[0]->ToString());

	if ((dp = dtrace_program_strcompile(dtp, *program,
	    DTRACE_PROBESPEC_NAME, 0, 0, NULL)) == NULL) {
		return (dtc->error("couldn't compile '%s': %s\n", *program,
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	if (dtrace_program_exec(dtp, dp, &info) == -1) {
		return (dtc->error("couldn't execute '%s': %s\n", *program,
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	return (Undefined());
}

Handle<Value>
DTraceConsumer::Setopt(const Arguments& args)
{
	DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(args.Holder());
	dtrace_hdl_t *dtp = dtc->dtc_handle;
	dtrace_prog_t *dp;
	dtrace_proginfo_t info;
	int rval;

	if (args.Length() < 1 || !args[0]->IsString())
		return (dtc->badarg("expected an option to set"));

	String::Utf8Value option(args[0]->ToString());

	if (args.Length() >= 2) {
		if (args[1]->IsArray())
			return (dtc->badarg("option value can't be an array"));

		if (args[1]->IsObject())
			return (dtc->badarg("option value can't be an object"));

		String::Utf8Value optval(args[1]->ToString());
		rval = dtrace_setopt(dtp, *option, *optval);
	} else {
		rval = dtrace_setopt(dtp, *option, NULL);
	}

	if (rval != 0) {
		return (dtc->error("couldn't set option '%s': %s\n", *option,
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	return (Undefined());
}

Handle<Value>
DTraceConsumer::Go(const Arguments& args)
{
	DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(args.Holder());
	dtrace_hdl_t *dtp = dtc->dtc_handle;

	if (dtrace_go(dtp) == -1) {
		return (dtc->error("couldn't enable tracing: %s\n",
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	return (Undefined());
}

Handle<Value>
DTraceConsumer::Stop(const Arguments& args)
{
	DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(args.Holder());
	dtrace_hdl_t *dtp = dtc->dtc_handle;

	if (dtrace_stop(dtp) == -1) {
		return (dtc->error("couldn't disable tracing: %s\n",
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	return (Undefined());
}

Local<Object>
DTraceConsumer::probedesc(const dtrace_probedesc_t *pd)
{
	Local<Object> probe = Object::New();
	probe->Set(String::New("provider"), String::New(pd->dtpd_provider));
	probe->Set(String::New("module"), String::New(pd->dtpd_mod));
	probe->Set(String::New("function"), String::New(pd->dtpd_func));
	probe->Set(String::New("name"), String::New(pd->dtpd_name));

	return (probe);
}

int
DTraceConsumer::bufhandler(const dtrace_bufdata_t *bufdata, void *arg)
{
	dtrace_probedata_t *data = bufdata->dtbda_probe;
	const dtrace_recdesc_t *rec = bufdata->dtbda_recdesc;
	DTraceConsumer *dtc = (DTraceConsumer *)arg;

	if (rec == NULL || rec->dtrd_action != DTRACEACT_PRINTF)
		return (DTRACE_HANDLE_OK);

	Local<Object> probe = dtc->probedesc(data->dtpda_pdesc);
	Local<Object> record = Object::New();
	record->Set(String::New("data"), String::New(bufdata->dtbda_buffered));
	Local<Value> argv[2] = { probe, record };

	dtc->dtc_callback->Call(dtc->dtc_args->This(), 2, argv);

	return (DTRACE_HANDLE_OK);
}

int
DTraceConsumer::consume(const dtrace_probedata_t *data,
    const dtrace_recdesc_t *rec, void *arg)
{
	DTraceConsumer *dtc = (DTraceConsumer *)arg;
	dtrace_probedesc_t *pd = data->dtpda_pdesc;
	Local<Value> datum;

	Local<Object> probe = dtc->probedesc(data->dtpda_pdesc);

	if (rec == NULL) {
		Local<Value> argv[1] = { probe };
		dtc->dtc_callback->Call(dtc->dtc_args->This(), 1, argv);
		return (DTRACE_CONSUME_NEXT);
	}

	if (!dtc->valid(rec)) {
		char errbuf[256];
	
		/*
		 * If this is a printf(), we'll defer to the bufhandler.
		 */
		if (rec->dtrd_action == DTRACEACT_PRINTF)
			return (DTRACE_CONSUME_THIS);

		dtc->dtc_error = dtc->error("unsupported action %s "
		    "in record for %s:%s:%s:%s\n",
		    dtc->action(rec, errbuf, sizeof (errbuf)),
		    pd->dtpd_provider, pd->dtpd_mod,
		    pd->dtpd_func, pd->dtpd_name);	
		return (DTRACE_CONSUME_ABORT);
	}

	Local<Object> record = Object::New();
	record->Set(String::New("data"), dtc->record(rec, data->dtpda_data));
	Local<Value> argv[2] = { probe, record };

	dtc->dtc_callback->Call(dtc->dtc_args->This(), 2, argv);

	return (DTRACE_CONSUME_THIS);
}

Handle<Value>
DTraceConsumer::Consume(const Arguments& args)
{
	DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(args.Holder());
	dtrace_hdl_t *dtp = dtc->dtc_handle;
	dtrace_workstatus_t status;

	if (!args[0]->IsFunction())
		return (dtc->badarg("expected function as argument"));

	dtc->dtc_callback = Local<Function>::Cast(args[0]);
	dtc->dtc_args = &args;
	dtc->dtc_error = Null();

	status = dtrace_work(dtp, NULL, NULL, DTraceConsumer::consume, dtc);

	if (status == -1 && !dtc->dtc_error->IsNull())
		return (dtc->dtc_error);

	return (Undefined());
}

/*
 * Caching the quantized ranges improves performance substantially if the
 * aggregations have many disjoing keys.  Note that we only cache a single
 * aggregation variable; programs that have more than one aggregation variable
 * may see significant degradations in performance.  (If this is a common
 * case, this cache should clearly be expanded.)
 */
Local<Array> *
DTraceConsumer::ranges_cached(dtrace_aggvarid_t varid)
{
	if (varid == dtc_ranges_varid)
		return (dtc_ranges);

	return (NULL);
}

Local<Array> *
DTraceConsumer::ranges_cache(dtrace_aggvarid_t varid, Local<Array> *ranges)
{
	if (dtc_ranges != NULL)
		delete [] dtc_ranges;

	dtc_ranges = ranges;
	dtc_ranges_varid = varid;

	return (ranges);
}

Local<Array> *
DTraceConsumer::ranges_quantize(dtrace_aggvarid_t varid)
{
	int64_t min, max;
	Local<Array> *ranges;
	int i;

	if ((ranges = ranges_cached(varid)) != NULL)
		return (ranges);

	ranges = new Local<Array>[DTRACE_QUANTIZE_NBUCKETS];

	for (i = 0; i < DTRACE_QUANTIZE_NBUCKETS; i++) {
		ranges[i] = Array::New(2);

		if (i < DTRACE_QUANTIZE_ZEROBUCKET) {
			/*
			 * If we're less than the zero bucket, our range
			 * extends from negative infinity through to the
			 * beginning of our zeroth bucket.
			 */
			min = i > 0 ? DTRACE_QUANTIZE_BUCKETVAL(i - 1) + 1 :
			    INT64_MIN;
			max = DTRACE_QUANTIZE_BUCKETVAL(i);
		} else if (i == DTRACE_QUANTIZE_ZEROBUCKET) {
			min = max = 0;
		} else {
			min = DTRACE_QUANTIZE_BUCKETVAL(i);
			max = i < DTRACE_QUANTIZE_NBUCKETS - 1 ?
			    DTRACE_QUANTIZE_BUCKETVAL(i + 1) - 1 :
			    INT64_MAX;
		}

		ranges[i]->Set(0, Number::New(min));
		ranges[i]->Set(1, Number::New(max));
	}

	return (ranges_cache(varid, ranges));
}

Local<Array> *
DTraceConsumer::ranges_lquantize(dtrace_aggvarid_t varid,
    const uint64_t arg)
{
	int64_t min, max;
	Local<Array> *ranges;
	int32_t base;
	uint16_t step, levels;
	int i;

	if ((ranges = ranges_cached(varid)) != NULL)
		return (ranges);

	base = DTRACE_LQUANTIZE_BASE(arg);
	step = DTRACE_LQUANTIZE_STEP(arg);
	levels = DTRACE_LQUANTIZE_LEVELS(arg);

	ranges = new Local<Array>[levels + 2];

	for (i = 0; i <= levels + 1; i++) {
		ranges[i] = Array::New(2);

		min = i == 0 ? INT64_MIN : base + ((i - 1) * step);
		max = i > levels ? INT64_MAX : base + (i * step) - 1;

		ranges[i]->Set(0, Number::New(min));
		ranges[i]->Set(1, Number::New(max));
	}

	return (ranges_cache(varid, ranges));
}

Local<Array> *
DTraceConsumer::ranges_llquantize(dtrace_aggvarid_t varid,
    const uint64_t arg, int nbuckets)
{
	int64_t value = 1, next, step;
	Local<Array> *ranges;
	int bucket = 0, order;
	uint16_t factor, low, high, nsteps;

	if ((ranges = ranges_cached(varid)) != NULL)
		return (ranges);

	factor = DTRACE_LLQUANTIZE_FACTOR(arg);
	low = DTRACE_LLQUANTIZE_LOW(arg);
	high = DTRACE_LLQUANTIZE_HIGH(arg);
	nsteps = DTRACE_LLQUANTIZE_NSTEP(arg);

	ranges = new Local<Array>[nbuckets];

	for (order = 0; order < low; order++)
		value *= factor;

	ranges[bucket] = Array::New(2);
	ranges[bucket]->Set(0, Number::New(0));
	ranges[bucket]->Set(1, Number::New(value - 1));
	bucket++;

	next = value * factor;
	step = next > nsteps ? next / nsteps : 1;

	while (order <= high) {
		ranges[bucket] = Array::New(2);
		ranges[bucket]->Set(0, Number::New(value));
		ranges[bucket]->Set(1, Number::New(value + step - 1));
		bucket++;

		if ((value += step) != next)
			continue;

		next = value * factor;
		step = next > nsteps ? next / nsteps : 1;
		order++;
	}

	ranges[bucket] = Array::New(2);
	ranges[bucket]->Set(0, Number::New(value));
	ranges[bucket]->Set(1, Number::New(INT64_MAX));

	assert(bucket + 1 == nbuckets);

	return (ranges_cache(varid, ranges));
}

int
DTraceConsumer::aggwalk(const dtrace_aggdata_t *agg, void *arg)
{
	DTraceConsumer *dtc = (DTraceConsumer *)arg;
	const dtrace_aggdesc_t *aggdesc = agg->dtada_desc;
	const dtrace_recdesc_t *aggrec;
	Local<Value> id = Integer::New(aggdesc->dtagd_varid), val;
	Local<Array> key;
	char errbuf[256];
	int i;

	/*
	 * We expect to have both a variable ID and an aggregation value here;
	 * if we have fewer than two records, something is deeply wrong.
	 */
	assert(aggdesc->dtagd_nrecs >= 2);
	key = Array::New(aggdesc->dtagd_nrecs - 2);

	for (i = 1; i < aggdesc->dtagd_nrecs - 1; i++) {
		const dtrace_recdesc_t *rec = &aggdesc->dtagd_rec[i];
		caddr_t addr = agg->dtada_data + rec->dtrd_offset;
		Local<Value> datum;

		if (!dtc->valid(rec)) {
			dtc->dtc_error = dtc->error("unsupported action %s "
			    "as key #%d in aggregation \"%s\"\n",
			    dtc->action(rec, errbuf, sizeof (errbuf)), i,
			    aggdesc->dtagd_name);
			return (DTRACE_AGGWALK_ERROR);
		}

		key->Set(i - 1, dtc->record(rec, addr));
	}

	aggrec = &aggdesc->dtagd_rec[aggdesc->dtagd_nrecs - 1];

	switch (aggrec->dtrd_action) {
	case DTRACEAGG_COUNT:
	case DTRACEAGG_MIN:
	case DTRACEAGG_MAX:
	case DTRACEAGG_SUM: {
		caddr_t addr = agg->dtada_data + aggrec->dtrd_offset;

		assert(aggrec->dtrd_size == sizeof (uint64_t));
		val = Number::New(*((int64_t *)addr));
		break;
	}

	case DTRACEAGG_AVG: {
		const int64_t *data = (int64_t *)(agg->dtada_data +
		    aggrec->dtrd_offset);

		assert(aggrec->dtrd_size == sizeof (uint64_t) * 2);
		val = Number::New(data[1] / (double)data[0]);
		break;
	}

	case DTRACEAGG_QUANTIZE: {
		Local<Array> quantize = Array::New();
		const int64_t *data = (int64_t *)(agg->dtada_data +
		    aggrec->dtrd_offset);
		Local<Array> *ranges, datum;
		int i, j = 0;

		ranges = dtc->ranges_quantize(aggdesc->dtagd_varid); 

		for (i = 0; i < DTRACE_QUANTIZE_NBUCKETS; i++) {
			if (!data[i])
				continue;

			datum = Array::New(2);
			datum->Set(0, ranges[i]);
			datum->Set(1, Number::New(data[i]));

			quantize->Set(j++, datum);
		}

		val = quantize;
		break;
	}

	case DTRACEAGG_LQUANTIZE:
	case DTRACEAGG_LLQUANTIZE: {
		Local<Array> lquantize = Array::New();
		const int64_t *data = (int64_t *)(agg->dtada_data +
		    aggrec->dtrd_offset);
		Local<Array> *ranges, datum;
		int i, j = 0;

		uint64_t arg = *data++;
		int levels = (aggrec->dtrd_size / sizeof (uint64_t)) - 1;

		ranges = (aggrec->dtrd_action == DTRACEAGG_LQUANTIZE ?
		    dtc->ranges_lquantize(aggdesc->dtagd_varid, arg) :
		    dtc->ranges_llquantize(aggdesc->dtagd_varid, arg, levels));

		for (i = 0; i < levels; i++) {
			if (!data[i])
				continue;

			datum = Array::New(2);
			datum->Set(0, ranges[i]);
			datum->Set(1, Number::New(data[i]));

			lquantize->Set(j++, datum);
		}

		val = lquantize;
		break;
	}

	default:
		dtc->dtc_error = dtc->error("unsupported aggregating action "
		    " %s in aggregation \"%s\"\n", dtc->action(aggrec, errbuf,
		    sizeof (errbuf)), aggdesc->dtagd_name);
		return (DTRACE_AGGWALK_ERROR);
	}

	Local<Value> argv[3] = { id, key, val };
	dtc->dtc_callback->Call(dtc->dtc_args->This(), 3, argv);

	return (DTRACE_AGGWALK_REMOVE);
}

Handle<Value>
DTraceConsumer::Aggwalk(const Arguments& args)
{
	HandleScope scope;
	DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(args.Holder());
	dtrace_hdl_t *dtp = dtc->dtc_handle;
	int rval;

	if (!args[0]->IsFunction())
		return (dtc->badarg("expected function as argument"));

	dtc->dtc_callback = Local<Function>::Cast(args[0]);
	dtc->dtc_args = &args;
	dtc->dtc_error = Null();

	if (dtrace_status(dtp) == -1) {
		return (dtc->error("couldn't get status: %s\n",
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	if (dtrace_aggregate_snap(dtp) == -1) {
		return (dtc->error("couldn't snap aggregate: %s\n",
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	rval = dtrace_aggregate_walk(dtp, DTraceConsumer::aggwalk, dtc);

	/*
	 * Flush the ranges cache; the ranges will go out of scope when the
	 * destructor for our HandleScope is called, and we cannot be left
	 * holding references.
	 */
	dtc->ranges_cache(DTRACE_AGGVARIDNONE, NULL);

	if (rval == -1) {
		if (!dtc->dtc_error->IsNull())
			return (dtc->dtc_error);

		return (dtc->error("couldn't walk aggregate: %s\n",
		    dtrace_errmsg(dtp, dtrace_errno(dtp))));
	}

	return (Undefined());
}

Handle<Value>
DTraceConsumer::Aggmin(const Arguments& args)
{
	return (Number::New(INT64_MIN));
}

Handle<Value>
DTraceConsumer::Aggmax(const Arguments& args)
{
	return (Number::New(INT64_MAX));
}

Handle<Value>
DTraceConsumer::Version(const Arguments& args)
{
	return (String::New(_dtrace_version));
}

extern "C" void
init (Handle<Object> target) 
{
	DTraceConsumer::Initialize(target);
}

NODE_MODULE(dtrace, init);
