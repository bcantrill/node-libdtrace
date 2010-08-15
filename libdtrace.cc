#include <v8.h>
#include <node.h>
#include <string.h>
#include <unistd.h>
#include <node_object_wrap.h>
#include <errno.h>
#include <string>
#include <vector>
#include <dtrace.h>

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

	static int consume(const dtrace_probedata_t *data,
	    const dtrace_recdesc_t *rec, void *arg);
	static int bufhandler(const dtrace_bufdata_t *bufdata, void *arg);

	static Handle<Value> New(const Arguments& args);
	static Handle<Value> Consume(const Arguments& args);
	static Handle<Value> Strcompile(const Arguments& args);
	static Handle<Value> Setopt(const Arguments& args);
	static Handle<Value> Go(const Arguments& args);
	static Handle<Value> Stop(const Arguments& args);

private:
	dtrace_hdl_t *dtc_handle;
	static Persistent<FunctionTemplate> dtc_templ;
	const Arguments *dtc_args;
	Local<Function> dtc_consume;
	Handle<Value> dtc_error;
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

	if (dtrace_handle_buffered(dtp, DTraceConsumer::bufhandler, NULL) == -1)
		throw (dtrace_errmsg(dtp, dtrace_errno(dtp)));
};

DTraceConsumer::~DTraceConsumer()
{
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
	NODE_SET_PROTOTYPE_METHOD(dtc_templ, "stop", DTraceConsumer::Stop);

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
		if (!args[1]->IsString())
			return (dtc->badarg("expected value for option"));

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

int
DTraceConsumer::bufhandler(const dtrace_bufdata_t *bufdata, void *arg)
{
	/*
	 * We do nothing here -- but should we wish to ever support complete
	 * dtrace(1) compatibility via node.js, we will need to do work here.
	 */
	return (DTRACE_HANDLE_OK);
}

int
DTraceConsumer::consume(const dtrace_probedata_t *data,
    const dtrace_recdesc_t *rec, void *arg)
{
	DTraceConsumer *dtc = (DTraceConsumer *)arg;
	dtrace_probedesc_t *pd = data->dtpda_pdesc;
	Local<Value> datum;

	Local<Object> probe = Object::New();
	probe->Set(String::New("provider"), String::New(pd->dtpd_provider));
	probe->Set(String::New("module"), String::New(pd->dtpd_mod));
	probe->Set(String::New("function"), String::New(pd->dtpd_func));
	probe->Set(String::New("name"), String::New(pd->dtpd_name));

	if (rec == NULL) {
		Local<Value> argv[1] = { probe };
		dtc->dtc_consume->Call(dtc->dtc_args->This(), 1, argv);
		return (DTRACE_CONSUME_NEXT);
	}

	if (rec->dtrd_action != DTRACEACT_DIFEXPR) {
		dtc->dtc_error = dtc->error("unsupported action type %d "
		    "in record for %s:%s:%s:%s\n", rec->dtrd_action,
		    pd->dtpd_provider, pd->dtpd_mod,
		    pd->dtpd_func, pd->dtpd_name);	
		return (DTRACE_CONSUME_ABORT);
	}

	Local<Object> record = Object::New();
	void *addr = data->dtpda_data;
	
	switch (rec->dtrd_size) {
	case sizeof (uint64_t):
		datum = Number::New(*((int64_t *)addr));
		break;

	case sizeof (uint32_t):
		datum = Integer::New(*((int32_t *)addr));
		break;

	case sizeof (uint16_t):
		datum = Integer::New(*((uint16_t *)addr));
		break;

	case sizeof (uint8_t):
		datum = Integer::New(*((uint8_t *)addr));
		break;

	default:
		datum = String::New((const char *)addr);
	}

	record->Set(String::New("data"), datum);

	Local<Value> argv[2] = { probe, record };

	dtc->dtc_consume->Call(dtc->dtc_args->This(), 2, argv);

	return (rec == NULL ? DTRACE_CONSUME_NEXT : DTRACE_CONSUME_THIS);
}

Handle<Value>
DTraceConsumer::Consume(const Arguments& args)
{
	DTraceConsumer *dtc = ObjectWrap::Unwrap<DTraceConsumer>(args.Holder());
	dtrace_hdl_t *dtp = dtc->dtc_handle;
	dtrace_workstatus_t status;

	if (!args[0]->IsFunction())
		return (dtc->badarg("expected function as argument"));

	dtc->dtc_consume = Local<Function>::Cast(args[0]);
	dtc->dtc_args = &args;
	dtc->dtc_error = Undefined();

	status = dtrace_work(dtp, NULL, NULL, DTraceConsumer::consume, dtc);

	if (status == -1 && !dtc->dtc_error->IsUndefined())
		return (dtc->dtc_error);

	return (Undefined());
}

extern "C" void
init (Handle<Object> target) 
{
	DTraceConsumer::Initialize(target);
}
