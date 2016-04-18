/*
 * Nexus.js - The next-gen JavaScript platform
 * Copyright (C) 2016  Abdullah A. Hassan <abdullah@webtomizer.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "nexus.h"
#include "value.h"
#include "task.h"

#include "globals/global.h"
#include "globals/console.h"
#include "globals/scheduler.h"
#include "globals/promise.h"
#include "globals/module.h"
#include "globals/loader.h"
#include "globals/filesystem.h"
#include "globals/context.h"

#include <boost/thread/pthread/mutex.hpp>

NX::Global::Global()
{

}

NX::Global::~Global()
{

}

constexpr JSClassDefinition NX::Global::InitGlobalClass()
{
  JSClassDefinition globalDef = kJSClassDefinitionEmpty;
  globalDef.className = "Global";
  globalDef.staticValues = NX::Global::GlobalProperties;
  globalDef.staticFunctions = NX::Global::GlobalFunctions;
  return globalDef;
}

boost::mutex timeoutsMutex;
boost::unordered_map<int, NX::AbstractTask *> globalTimeouts;

JSStaticFunction NX::Global::GlobalFunctions[] {
  { "setTimeout",
    [](JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
      NX::Context * context = Context::FromJsContext(ctx);
      NX::Nexus * nx = context->nexus();
      if (argumentCount < 2) {
        NX::Value message(ctx, "invalid arguments");
        JSValueRef args[] { message.value(), nullptr };
        *exception = JSObjectMakeError(ctx, 1, args, nullptr);
      }
      try {
        NX::Value timeout(ctx, arguments[1]);
        std::vector<JSValueRef> saved { arguments[0], arguments[1] };
        std::vector<JSValueRef> args;
        JSValueProtect(context->toJSContext(), arguments[0]);
        for(int i = 2; i < argumentCount; i++) {
          JSValueProtect(context->toJSContext(), arguments[i]);
          args.push_back(arguments[i]);
        }
        NX::AbstractTask * task = nx->scheduler()->scheduleTask(boost::posix_time::milliseconds(timeout.toNumber()), [=]() {
          JSValueRef exp = nullptr;
          JSValueRef ret = JSObjectCallAsFunction(context->toJSContext(), JSValueToObject(context->toJSContext(), saved[0], &exp),
                                                  nullptr, args.size(), &args[0], &exp);
          if (exp) {
            NX::Nexus::ReportException(context->toJSContext(), exp);
          }
          for(auto i : args) {
            JSValueUnprotect(context->toJSContext(), i);
          }
          JSValueUnprotect(context->toJSContext(), saved[0]);
        });
        {
          boost::mutex::scoped_lock lock(timeoutsMutex);
          int id = 1;
          while(globalTimeouts.find(id) != globalTimeouts.end()) id++;
          globalTimeouts[id] = task;
          return JSValueMakeNumber(ctx, id);
        }
      } catch(const std::exception & e) {
        NX::Value message(ctx, e.what());
        JSValueRef args[] { message.value(), nullptr };
        *exception = JSObjectMakeError(ctx, 1, args, nullptr);
      }
      return JSValueMakeUndefined(ctx);
    }, 0
  },
  { "clearTimeout",
    [](JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount,
       const JSValueRef arguments[], JSValueRef* exception) -> JSValueRef {
      NX::Context * context = Context::FromJsContext(ctx);
      NX::Nexus * nx = context->nexus();
      if (argumentCount != 1) {
        NX::Value message(ctx, "invalid arguments");
        JSValueRef args[] { message.value(), nullptr };
        *exception = JSObjectMakeError(ctx, 1, args, nullptr);
      }
      try {
        NX::Value timeoutId(ctx, arguments[0]);
        int taskId = (int)timeoutId.toNumber();
        {
          boost::mutex::scoped_lock lock(timeoutsMutex);
          if (NX::AbstractTask * task = globalTimeouts[taskId]) {
            task->abort();
          } else {
            /* TODO error */
          }
          globalTimeouts.erase(taskId);
        }
      } catch(const std::exception & e) {
        NX::Value message(ctx, e.what());
        JSValueRef args[] { message.value(), nullptr };
        *exception = JSObjectMakeError(ctx, 1, args, nullptr);
      }
      return JSValueMakeUndefined(ctx);
    }, 0
  },
  { nullptr, nullptr, 0 }
};

JSStaticValue NX::Global::GlobalProperties[] {
  NX::Globals::Console::GetStaticProperty(),
  NX::Globals::Scheduler::GetStaticProperty(),
  NX::Globals::Promise::GetStaticProperty(),
  NX::Globals::FileSystem::GetStaticProperty(),
  NX::Globals::Context::GetStaticProperty(),
  NX::Globals::Module::GetStaticProperty(),
  NX::Globals::Loader::GetStaticProperty(),
  { nullptr, nullptr, nullptr, 0 }
};

JSClassDefinition NX::Global::GlobalClass = NX::Global::InitGlobalClass();