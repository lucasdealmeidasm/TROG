#define JSENGINE_CPP

#include "duktape.h"

#include "JsEngine.h"

static void FatalHandler(void *udata, const char *msg)
{
	((JsEngine *) udata)->err = JsEngine::ErrFatal;
	((JsEngine *) udata)->OnError(std::string(msg));
}

JsEngine::JsEngine()
{
	heapUdata = (void *) this;
	ctx = duk_create_heap(0, 0, 0, heapUdata, FatalHandler);
	state = StateNone;
	numArgs = 0;
}

void JsEngine::OnError(std::string msg)
{
}

#define BEGIN_PUSH													\
	if(state != StateCall && state != StateSet) err = ErrWhyPush;	\
	if(err) return this;											\
	numArgs++;
#define END_PUSH							\
	if(state == StateSet) return EndSet();	\
	return this;

JsEngine *JsEngine::PushNumber(double d)
{
	BEGIN_PUSH
	duk_push_number(ctx, d);
	END_PUSH
}

#define PUSH_BUFFER(type)											\
	duk_push_external_buffer(ctx);									\
	duk_config_buffer(ctx, -1, b, len * sizeof(*b));				\
	duk_push_buffer_object(ctx, -1, 0, len * sizeof(*b), type);     \
	duk_swap_top(ctx, -2);											\
	duk_pop(ctx);

JsEngine *JsEngine::PushBuffer(unsigned int *b, int len)
{
	BEGIN_PUSH
	PUSH_BUFFER(DUK_BUFOBJ_UINT32ARRAY)
	END_PUSH
}

JsEngine *JsEngine::PushBuffer(int *b, int len)
{
	BEGIN_PUSH
	PUSH_BUFFER(DUK_BUFOBJ_INT32ARRAY)
	END_PUSH
}

JsEngine *JsEngine::PushString(std::string s)
{
	BEGIN_PUSH
	duk_push_string(ctx, s.c_str());
	END_PUSH
}

JsEngine *JsEngine::PushTable(std::map<std::string, std::string> *m)
{
	BEGIN_PUSH
	duk_push_object(ctx);
	for(std::map<std::string, std::string>::iterator it = m->begin(); it != m->end(); it++)
	{
		duk_push_string(ctx, it->second.c_str());
		duk_put_prop_string(ctx, -1, it->first.c_str());
	}
	END_PUSH
}

JsEngine *JsEngine::PushCFunction(void *f, int nargs)
{
	BEGIN_PUSH
	duk_push_c_function(ctx, (duk_c_function) f, nargs);
	END_PUSH
}

#define BEGIN_POP_					\
	if(OnPop()) return err;
#define BEGIN_POP(typ)				\
	BEGIN_POP_						\
	if(!duk_is_ ## typ(ctx, -1))	\
	{								\
		return (err = ErrPopType);	\
	}
#define END_POP						\
	OnEnd();						\
	return ErrNone;

JsEngine::JsEngineError JsEngine::PopNumberTo(double *d)
{
	BEGIN_POP(number)
	*d = duk_get_number(ctx, -1);
	duk_pop(ctx);
	END_POP
}

JsEngine::JsEngineError JsEngine::PopStringTo(std::string *s)
{
	BEGIN_POP(string)
	*s = duk_get_string(ctx, -1);
	duk_pop(ctx);
	END_POP
}

JsEngine::JsEngineError JsEngine::PopTableTo(std::map<std::string, std::string> *m)
{
	BEGIN_POP(object)
	duk_enum(ctx, -1, 0);
	while(duk_next(ctx, -1, 1))
	{
		(*m)[duk_get_string(ctx, -2)] = duk_get_string(ctx, -1);
		duk_pop_2(ctx);
	}
	duk_pop_2(ctx);
	END_POP
}

JsEngine::JsEngineError JsEngine::Pop()
{
	BEGIN_POP_
	duk_pop(ctx);
	END_POP
}

void JsEngine::OnEnd()
{
	state = StateNone;
}

void JsEngine::ClearError()
{
	err = ErrNone;
	while(duk_get_top(ctx)) duk_pop(ctx);
}

static std::string JsErrorToString(duk_context *ctx)
{
	const char *file, *name, *message;
	char errorString[4096];
	int lineNumber;
	
	duk_get_prop_string(ctx, -1, "fileName");
	file = duk_get_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "lineNumber");
	lineNumber = duk_get_int(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "name");
	name = duk_get_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "message");
	message = duk_get_string(ctx, -1);
	duk_pop(ctx);
	
	snprintf(errorString, sizeof(errorString), "%-12s:%-4d:   %s: %s", file, lineNumber, name, message);
	errorString[sizeof(errorString) - 1] = 0;
	
	return std::string(errorString);
}

int JsEngine::OnPop()
{
	if(err) return 1;
	switch(state)
	{
		case StateCall:
			if(duk_pcall(ctx, numArgs))
			{
				err = ErrCall;
				OnError(JsErrorToString(ctx));
				duk_pop(ctx);
				return 1;
			}
			return 0;
		case StateEval:
			if(duk_peval_string(ctx, toEval.c_str()))
			{
				err = ErrEval;
				OnError(duk_safe_to_string(ctx, -1));
				duk_pop(ctx);
				return 1;
			}
			return 0;
		case StateNone:
			err = ErrNothingToDo;
			return 1;
	}
	return 0;
}

JsEngine *JsEngine::Eval(std::string c)
{
	if(state == StateNone)
	{
		state = StateEval;
		toEval = c;
	}
	else
	{
		err = ErrBusy;
	}
	return this;
}

JsEngine *JsEngine::Call(std::string f)
{
	if(state == StateNone)
	{
		duk_get_global_string(ctx, f.c_str());
		if(!duk_is_callable(ctx, -1))
		{
			err = ErrNotCallable;
			duk_pop(ctx);
		}
		else
		{
			numArgs = 0;
			state = StateCall;
		}
	}
	else
	{
		err = ErrBusy;
	}
	return this;
}

JsEngine *JsEngine::Set(std::string n)
{
	if(state == StateNone)
	{
		state = StateSet;
		toSet = n;
	}
	else
	{
		err = ErrBusy;
	}
	return this;
}

JsEngine *JsEngine::EndSet()
{
	if(duk_get_top(ctx) != 0)
	{
		err = ErrSetNoValue;
		return 0;
	}
	duk_put_global_string(ctx, toSet.c_str());
	state = StateNone;
	return this;
}
