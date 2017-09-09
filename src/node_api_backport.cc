#include "node_api_backport.h"

using v8::Function;
using v8::HandleScope;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::TryCatch;
using v8::Value;

namespace node {

CallbackScope::CallbackScope(v8::Isolate *_isolate,
                             v8::Local<v8::Object> _object,
                             node::async_context context) :
    isolate(_isolate),
    env(node::Environment::GetCurrent(isolate->GetCurrentContext())),
    _try_catch(isolate),
    object(_object),
    callback_scope(
        node::Environment::GetCurrent(isolate->GetCurrentContext())) {
  Local<Function> pre_fn = env->async_hooks_pre_function();

  Local<Value> async_queue_v = object->Get(env->async_queue_string());
  if (async_queue_v->IsObject())
    ran_init_callback = true;

  if (ran_init_callback && !pre_fn.IsEmpty()) {
    TryCatch try_catch(env->isolate());
    MaybeLocal<Value> ar = pre_fn->Call(env->context(), object, 0, nullptr);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
    }
  }
}

CallbackScope::~CallbackScope() {
  Local<Function> post_fn = env->async_hooks_post_function();
  if (ran_init_callback && !post_fn.IsEmpty()) {
    Local<Value> did_throw = v8::Boolean::New(isolate, _try_catch.HasCaught());
    // Currently there's no way to retrieve an uid from node::MakeCallback().
    // This needs to be fixed.
    Local<Value> vals[] =
        { Undefined(env->isolate()).As<Value>(), did_throw };
    TryCatch try_catch(env->isolate());
    MaybeLocal<Value> ar =
        post_fn->Call(env->context(), object, arraysize(vals), vals);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
      return;
    }
  }

  if (callback_scope.in_makecallback()) {
    return;
  }

  Environment::TickInfo* tick_info = env->tick_info();

  if (tick_info->length() == 0) {
    env->isolate()->RunMicrotasks();
  }

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return;
  }

  env->tick_callback_function()->Call(env->process_object(), 0, nullptr);
}

AsyncResource::AsyncResource(v8::Isolate* _isolate,
                             v8::Local<v8::Object> _object,
                             v8::Local<v8::String> name) : isolate(_isolate) {
  object.Reset(isolate, _object);
}

AsyncResource::~AsyncResource() {
  object.Reset();
}

}  // end of namespace node

CallbackScope::CallbackScope(node::AsyncResource* work) :
    scope(work->isolate,
          v8::Local<Object>::New(work->isolate,
          work->object), {0, 0}) {}