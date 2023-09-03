#include "HostProxy.h"

#include <memory>

#include "JSIValueConverter.h"
#include <unordered_set>

namespace qjs
{

  JSClassExoticMethods HostObjectExoticMethods;
  JSClassID HostObjectProxy::kJSClassID = 0;
  JSClassDef HostObjectProxy::kJSClassDef;

  void *OpaqueData::GetHostData(JSContext *context, JSValueConst this_val, JSClassID cls)
  {
    auto opaqueData =
        reinterpret_cast<OpaqueData *>(JS_GetOpaque2(context, this_val, cls));

    if (opaqueData)
    {
      return opaqueData->hostData_;
    }

    return nullptr;
  }

  void *OpaqueData::GetHostData(JSValueConst this_val, JSClassID cls)
  {
    auto opaqueData =
        reinterpret_cast<OpaqueData *>(JS_GetOpaque(this_val,
                                                    cls));

    if (opaqueData)
    {
      return opaqueData->hostData_;
    }

    return nullptr;
  }

  HostObjectProxy::HostObjectProxy(
      QuickJSRuntime &runtime,
      std::shared_ptr<jsi::HostObject> hostObject)
      : runtime_(runtime), hostObject_(hostObject)
  {
    opaqueData_.hostData_ = this;
  }

  std::shared_ptr<jsi::HostObject> HostObjectProxy::GetHostObject()
  {
    return hostObject_;
  }

  OpaqueData *HostObjectProxy::GetOpaqueData()
  {
    return &opaqueData_;
  }

  JSClassID HostObjectProxy::GetClassID()
  {
    if (!kJSClassID)
    {
      JS_NewClassID(&kJSClassID);
    }
    return kJSClassID;
  }

  JSValue
  HostObjectProxy::Get(JSContext *ctx, JSValueConst this_val, JSAtom name,
                       JSValueConst receiver)
  {
    auto *hostObjectProxy =
        reinterpret_cast<HostObjectProxy *>(OpaqueData::GetHostData(this_val,
                                                                    HostObjectProxy::GetClassID()));

    assert(hostObjectProxy);

    QuickJSRuntime &runtime = hostObjectProxy->runtime_;
    jsi::PropNameID sym = JSIValueConverter::ToJSIPropNameID(runtime, name);
    // JS_FreeAtom(ctx, name);
    jsi::Value ret;
    try
    {
      ret = hostObjectProxy->hostObject_->get(runtime, sym);
    }
    catch (const jsi::JSError &error)
    {
      JS_Throw(ctx, JSIValueConverter::ToJSValue(runtime, error.value()));
      return JS_UNDEFINED;
    }
    catch (const std::exception &ex)
    {
      return JS_UNDEFINED;
    }
    catch (...)
    {
      return JS_UNDEFINED;
    }
    return JSIValueConverter::ToJSValue(runtime, ret);
  }

  int HostObjectProxy::Set(
      JSContext *ctx,
      JSValueConst this_val,
      JSAtom name,
      JSValue val,
      JSValueConst receiver,
      int flags)
  {
    auto *hostObjectProxy =
        reinterpret_cast<HostObjectProxy *>(OpaqueData::GetHostData(this_val,
                                                                    HostObjectProxy::GetClassID()));
    assert(hostObjectProxy);

    QuickJSRuntime &runtime = hostObjectProxy->runtime_;
    jsi::PropNameID sym = JSIValueConverter::ToJSIPropNameID(runtime, name);
    // JS_FreeAtom(ctx, name);
    try
    {
      hostObjectProxy->hostObject_->set(
          runtime, sym, JSIValueConverter::ToJSIValue(runtime, val));
    }
    catch (const jsi::JSError &error)
    {
      JS_Throw(ctx, JSIValueConverter::ToJSValue(runtime, error.value()));
      return 0;
    }
    catch (const std::exception &ex)
    {
      return 0;
    }
    catch (...)
    {
      return 0;
    }
    return 1;
  }

  int HostObjectProxy::GetOwnPropertyNames(JSContext *ctx, JSPropertyEnum **ptab, uint32_t *plen,
                                           JSValueConst this_val)
  {
    *ptab = nullptr;
    *plen = 0;

    auto *hostObjectProxy =
        reinterpret_cast<HostObjectProxy *>(OpaqueData::GetHostData(this_val,
                                                                    HostObjectProxy::GetClassID()));

    assert(hostObjectProxy);
    QuickJSRuntime &runtime = hostObjectProxy->runtime_;

    auto names = hostObjectProxy->hostObject_->getPropertyNames(runtime);

    try
    {
      if (!names.empty())
      {
        *ptab = (JSPropertyEnum *)js_malloc(ctx, names.size() * sizeof(JSPropertyEnum));
        *plen = names.size();
        for (size_t i = 0; i < names.size(); ++i)
        {
          (*ptab + i)->atom = JS_NewAtom(ctx, names[i].utf8(runtime).c_str());
          (*ptab + i)->is_enumerable = 1;
        }
      }

      return 0; // Must return 0 on success
    }
    catch (const jsi::JSError &jsError)
    {
      JS_Throw(ctx, JSIValueConverter::ToJSValue(runtime, jsError.value()));
      return -1;
    }
    catch (const std::exception &ex)
    {
      return -1;
    }
    catch (...)
    {
      return -1;
    }
  }

  void HostObjectProxy::Finalizer(JSRuntime *rt, JSValue val)
  {
    auto hostObjectProxy =
        reinterpret_cast<HostObjectProxy *>(OpaqueData::GetHostData(val,
                                                                    HostObjectProxy::GetClassID()));
    assert(hostObjectProxy->hostObject_.use_count() == 1);
    hostObjectProxy->opaqueData_.nativeState_ = nullptr;
    delete hostObjectProxy;
  }

  void HostObjectProxy::RegisterClass(QuickJSRuntime &runtime)
  {
    if (!JS_IsRegisteredClass(runtime.getJSRuntime(), GetClassID()))
    {

      HostObjectExoticMethods = {};
      HostObjectExoticMethods.get_property = HostObjectProxy::Get,
      HostObjectExoticMethods.set_property = HostObjectProxy::Set,
      HostObjectExoticMethods.get_own_property_names = HostObjectProxy::GetOwnPropertyNames;

      HostObjectProxy::kJSClassDef = {
          .class_name = "HostObjectProxy",
          .finalizer = &HostObjectProxy::Finalizer,
          .exotic = &HostObjectExoticMethods};

      JS_NewClass(runtime.getJSRuntime(), GetClassID(), &kJSClassDef);
    }
  }

  JSClassID HostFunctionProxy::kJSClassID = 0;

  JSClassDef HostFunctionProxy::kJSClassDef = {
      .class_name = "HostFunctionProxy",
      .finalizer = &HostFunctionProxy::Finalizer,
      .call = &HostFunctionProxy::FunctionCallback,
  };

  HostFunctionProxy::HostFunctionProxy(
      QuickJSRuntime &runtime,
      jsi::HostFunctionType &&hostFunction)
      : runtime_(runtime), hostFunction_(std::move(hostFunction))
  {
    opaqueData_.hostData_ = this;
  }

  jsi::HostFunctionType &HostFunctionProxy::GetHostFunction()
  {
    return hostFunction_;
  }

  OpaqueData *HostFunctionProxy::GetOpaqueData()
  {
    return &opaqueData_;
  }

  void HostFunctionProxy::Finalizer(JSRuntime *rt, JSValue val)
  {
    auto hostFunctionProxy =
        reinterpret_cast<HostFunctionProxy *>(OpaqueData::GetHostData(val,
                                                                      HostFunctionProxy::GetClassID()));
    hostFunctionProxy->opaqueData_.nativeState_ = nullptr;
    delete hostFunctionProxy;
  }

  JSClassID HostFunctionProxy::GetClassID()
  {
    if (!kJSClassID)
    {
      JS_NewClassID(&kJSClassID);
    }
    return kJSClassID;
  }

  void HostFunctionProxy::RegisterClass(QuickJSRuntime &runtime)
  {
    if (!JS_IsRegisteredClass(runtime.getJSRuntime(), HostFunctionProxy::GetClassID()))
    {
      JS_NewClass(runtime.getJSRuntime(), HostFunctionProxy::GetClassID(),
                  &HostFunctionProxy::kJSClassDef);
    }
  }

  JSValue HostFunctionProxy::FunctionCallback(
      JSContext *ctx,
      JSValueConst func_obj,
      JSValueConst val,
      int argc,
      JSValueConst *argv,
      int flags)
  {
    auto *hostFunctionProxy =
        reinterpret_cast<HostFunctionProxy *>(OpaqueData::GetHostData(func_obj,
                                                                      HostFunctionProxy::GetClassID()));

    auto &runtime = hostFunctionProxy->runtime_;

    const unsigned maxStackArgCount = 8;
    jsi::Value stackArgs[maxStackArgCount];
    std::unique_ptr<jsi::Value[]> heapArgs;
    jsi::Value *args;
    if (argc > maxStackArgCount)
    {
      heapArgs = std::make_unique<jsi::Value[]>(argc);
      for (size_t i = 0; i < argc; i++)
      {
        heapArgs[i] = JSIValueConverter::ToJSIValue(runtime, argv[i]);
      }
      args = heapArgs.get();
    }
    else
    {
      for (size_t i = 0; i < argc; i++)
      {
        stackArgs[i] = JSIValueConverter::ToJSIValue(runtime, argv[i]);
      }
      args = stackArgs;
    }

    jsi::Value thisVal(JSIValueConverter::ToJSIValue(runtime, val));
    try
    {
      return JSIValueConverter::ToJSValue(
          runtime,
          hostFunctionProxy->hostFunction_(runtime, thisVal, args, argc));
    }
    catch (const jsi::JSError &error)
    {
      JS_Throw(ctx, JSIValueConverter::ToJSValue(runtime, error.value()));
      return JS_UNDEFINED;
    }
    catch (const std::exception &ex)
    {
      return JS_UNDEFINED;
    }
    catch (...)
    {
      return JS_UNDEFINED;
    }
  }

} // namespace qjs
