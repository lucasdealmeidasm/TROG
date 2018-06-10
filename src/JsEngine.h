#ifndef JSENGINE_H
#define JSENGINE_H

#include <map>
#include <string>

#ifndef JSENGINE_CPP
struct duk_context;
#endif

class JsEngine
{
	public:
		enum JsEngineError
		{
			ErrNone = 0,
			ErrFatal,
			ErrPopType,
			ErrCall,
			ErrEval,
			ErrNothingToDo,
			ErrBusy,
			ErrNotCallable,
			ErrWhyPush,
			ErrSetNoValue
		};
		
		JsEngine *PushNumber(double d);
		JsEngine *PushBuffer(unsigned int *b, int len);
		JsEngine *PushBuffer(int *b, int len);
		JsEngine *PushString(std::string s);
		JsEngine *PushTable(std::map<std::string, std::string> *m);
		JsEngine *PushCFunction(void *f, int nargs);
		
		JsEngineError PopNumberTo(double *d);
		JsEngineError PopStringTo(std::string *s);
		JsEngineError PopTableTo(std::map<std::string, std::string> *m);
		JsEngineError Pop();
		
		JsEngine *Eval(std::string c);
		JsEngine *Call(std::string f);
		JsEngine *Set(std::string n);
		
		virtual void OnError(std::string msg);
		
		void ClearError();
		
		JsEngine();
		
		JsEngineError err;
	private:
		duk_context *ctx;
		void *heapUdata;
		int numArgs;
		std::string toEval, toSet;
		enum JsEngineState
		{
			StateNone,
			StateCall,
			StateEval,
			StateSet
		};
		JsEngineState state;
		
		int OnPop();
		void OnEnd();
		JsEngine *EndSet();
};

#endif