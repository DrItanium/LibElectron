/*
 *
 * Copyright (c) 2015-2016 Parasoft Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#include "Environment.h"
extern "C" {
#include "clips.h"
}

namespace Neutron
{

Environment::Environment() : reclaim(true)
{
    env = CreateEnvironment();
    if (!env) {
        throw Problem("Could not create a CLIPS environment!");
    }
}


Environment::Environment(void* theEnv) : reclaim(false), env(theEnv) { }

Environment::~Environment()
{
    if (reclaim && env && DestroyEnvironment(env) == FALSE) {
        throw Problem("Could not destroy the given CLIPS environment!"); // parasoft-suppress EXCEPT-03-1 "This is a terminate execution condition because something REALLY bad happened!"
    }
}

void
Environment::applyToFunction(std::function<void(void*)> fn)
{
    fn(env);
}

void
Environment::funcall(const std::string& functionName, const std::string& args, DataObjectPtr obj)
{
    // According to the documentation, an integer is returned from
    // EnvFunctionCall where:
    // 1 - an error occured
    // 0 - success
    //
    // So we check if a zero (which is FALSE in clips....) was returned which
    // means that the microcode function was executed successfully.
    if (::EnvFunctionCall(env, functionName.c_str(), args.c_str(), obj) != 0) {
        std::stringstream ss;
        ss << "Function call of (" << functionName << " " << args << ") failed!";
        auto tmp = ss.str();
        throw Problem(tmp);
    }
}



void
Environment::funcall(const std::string& functionName, const std::string& args)
{
    DataObject nil;
    funcall(functionName, args, &nil);
}

void
Environment::funcall(const std::string& functionName)
{
    funcall(functionName, "");
}

void
Environment::funcall(const std::string& functionName, DataObjectPtr ret)
{
    funcall(functionName, "", ret);
}

void
Environment::funcall(const std::string& functionName, const std::string& args, std::string& ret)
{
    DataObject dat;
    funcall(functionName, args, &dat);
    // if we get here then we had a successful execution (this is really unsafe
    // at this point since the return type may have changed)
    /// @todo add type checking support
    extractValue(&dat, ret);
}


void
Environment::funcall(const std::string& functionName, const std::string& args, bool& ret)
{
    DataObject dat;
    funcall(functionName, args, &dat);
    extractValue(&dat, ret);
}


void
Environment::reset()
{
    ::EnvReset(env);
}

int64
Environment::run(int64 count)
{
    return ::EnvRun(env, count);
}


void
Environment::loadFile(const std::string& path)
{
    std::stringstream st;
    switch (::EnvLoad(env, path.string().c_str())) {
    case -1: // parse error
        st << "Unable to parse file: " << path;
        throw Problem(st.str());
        break;
    case 0:
        st << "Unable to load file: " << path;
        throw Problem(st.str());
        break;
    default:
        break;
    }
}

void*
Environment::getRawEnvironment()
{
    return env;
}

void
Environment::watch(const std::string& value)
{
    if (::EnvWatch(env, value.c_str()) == 0) {
        std::stringstream ss;
        ss << "Attempting to watch '" << value << "' was not successful!";
        auto str = ss.str();
        throw Problem(str);
    }
}

void
Environment::unwatch(const std::string& value)
{
    if (::EnvUnwatch(env, value.c_str()) == 0) {
        std::stringstream ss;
        ss << "Attempting to unwatch '" << value << "' was not successful!";
        auto str = ss.str();
        throw Problem(str);
    }
}

void*
Environment::addSymbol(const std::string& symbol)
{
    return (void*)::EnvAddSymbol(env, symbol.c_str());
}

void*
Environment::addSymbol(const char* symbol)
{
    return (void*)::EnvAddSymbol(env, symbol);
}

void*
Environment::addNumber(int32 number)
{
    return (void*)::EnvAddLong(env, number);
}

void*
Environment::addNumber(int64 number)
{
    return (void*)::EnvAddLong(env, number);
}

void*
Environment::addNumber(uint32 number)
{
    return (void*)::EnvAddLong(env, number);
}

void*
Environment::addNumber(double number)
{
    return (void*)::EnvAddDouble(env, number);
}

void*
Environment::addNumber(float number)
{
    return (void*)::EnvAddDouble(env, number);
}

void*
Environment::makeInstance(const std::string& instanceString)
{
    void* result = (void*)::EnvMakeInstance(env, instanceString.c_str());
    if (!result) {
        std::stringstream ss;
        ss << "Couldn't construct an instance from '" << instanceString << "'!";
        auto str = ss.str();
        throw Problem(str);
    } else {
        return result;
    }
}

void
Environment::setSlot(void* instance, const std::string& slotName, DataObjectPtr value)
{
    if (::EnvDirectPutSlot(env, instance, slotName.c_str(), value) == 0) {
        /// @todo add support for printing out the data object value
        throw Problem("Attempting to set slot '" << slotName << "' failed!");
    }
}

void
Environment::getSlot(void* instance, const std::string& slotName, DataObjectPtr ret)
{
    ::EnvDirectGetSlot(env, instance, slotName.c_str(), ret);
}

void
Environment::installExpression(EXPRESSION* expr)
{
    ::ExpressionInstall(env, expr);
}

void
Environment::deinstallExpression(EXPRESSION* expr)
{
    ::ExpressionDeinstall(env, expr);
}

bool
Environment::evaluateExpression(EXPRESSION* expr, DataObjectPtr ret)
{
    // check and see if we did not get an evaluation error
    return ::EvaluateExpression(env, expr, ret) != TRUE;
}

void
Environment::reclaimExpressionList(EXPRESSION* expr)
{
    ::ReturnExpression(env, expr);
}

EXPRESSION*
Environment::generateConstantExpression(uint16 type, void* value)
{
    return ::GenConstant(env, type, value);
}

bool
Environment::generateFunctionExpression(const std::string& name, FUNCTION_REFERENCE* ref)
{
    return ::GetFunctionReference(env, name.c_str(), ref) == TRUE;
}

void
Environment::extractValue(DataObjectPtr dobj, std::function<void(Environment*, DataObjectPtr)> fn)
{
    fn(this, dobj);
}


void
Environment::unmakeInstance(void* instancePtr)
{
    ::EnvUnmakeInstance(env, instancePtr);
}

void
Environment::buildAndExecuteFunction(const std::string& function)
{
    DataObject dontCare;
    buildAndExecuteFunction(function, &dontCare);
}

void
Environment::buildAndExecuteFunction(const std::string& function, DataObjectPtr ret)
{
    FunctionBuilder fb(this);
    fb.setFunctionReference(function);
    fb.invoke(ret);
}

int
Environment::installExternalAddressType(Environment::ExternalAddressType* description)
{
    return ::InstallExternalAddressType(env, description);
}

bool
Environment::checkArgument(const std::string& function, int position, int type, DataObjectPtr ptr)
{
    return EnvArgTypeCheck(env, function.c_str(), position, type, ptr) != FALSE;
}

bool
Environment::checkArgument(const std::string& function, int position, DataObjectType type, DataObjectPtr ptr)
{
    return checkArgument(function, position, static_cast<int>(type), ptr);
}

bool
Environment::argumentIsExternalAddress(const std::string& function, int position, DataObjectPtr ptr)
{
    return checkArgument(function, position, EXTERNAL_ADDRESS, ptr);
}

bool
Environment::installUserFunction(const std::string& name, int returnType, Environment::RawFunction body, const std::string& actualName, const char* restrictions)
{
    return ::EnvDefineFunction2(env, name.c_str(), returnType, body, actualName.c_str(), restrictions);
}

bool
Environment::installUserFunction(const std::string& name, UserFunctionReturnType returnType, Environment::RawFunction body, const std::string& actualName, const char* restrictions)
{
   return installUserFunction(name, static_cast<int>(returnType), body, actualName, restrictions);
}

void*
Environment::createMultifield(int32 size)
{
    return ::EnvCreateMultifield(env, size);
}

int
Environment::getArgumentCount() const
{
    return ::EnvRtnArgCount(env);
}

// End Environment stuff

// Begin FunctionBuilder stuff
FunctionBuilder::FunctionBuilder(Environment* e) : env(e) { }

FunctionBuilder::~FunctionBuilder()
{
    env->deinstallExpression(&_ref);
    env->reclaimExpressionList(_ref.argList);
    _ref.argList = nullptr;
    curr = nullptr;
    env = nullptr;
}

void
FunctionBuilder::setFunctionReference(const std::string& func)
{
    if (!env->generateFunctionExpression(func, &_ref)) {
        std::stringstream ss;
        ss << "Function " << func << " does not exist!!!!";
        auto str = ss.str();
        throw Problem(str);
    } else {
        _function = func;
        functionReferenceSet = true;
    }
}

void
FunctionBuilder::installArgument(uint16 type, void* value)
{
    if (functionReferenceSet) {
        auto tmp = env->generateConstantExpression(type, value);
        env->installExpression(tmp);
        if (_ref.argList == nullptr) {
            _ref.argList = tmp;
            curr = tmp;
        } else {
            curr->nextArg = tmp;
            curr = tmp;
        }
    } else {
        throw Problem("ERROR: Attempted to build an argument list before setting the target function!");
    }
}

void
FunctionBuilder::installArgument(DataObjectType type, void* value)
{
    installArgument(static_cast<uint16>(type), value);
}

void
FunctionBuilder::addArgument(bool value)
{
    installArgument(SYMBOL, value ? (void*)::EnvTrueSymbol(env->getRawEnvironment()) : (void*)::EnvFalseSymbol(env->getRawEnvironment()));
}

void
FunctionBuilder::addArgument(int64 value)
{
    installArgument(INTEGER, env->addNumber(value));
}

void
FunctionBuilder::addArgument(float value)
{
    installArgument(FLOAT, env->addNumber(value));
}

void
FunctionBuilder::addArgument(double value)
{
    installArgument(FLOAT, env->addNumber(value));
}

void
FunctionBuilder::addArgument(const char* str)
{
    installArgument(STRING, env->addSymbol(str));
}

void
FunctionBuilder::addArgument(const std::string& str)
{
    addArgument(str.c_str());
}

void
FunctionBuilder::invoke(DataObjectPtr ret)
{
    if (!functionReferenceSet) {
        throw Problem("ERROR: attempted to invoke a function builder without setting its function!");
    } else if (!env->evaluateExpression(&_ref, ret)) {
        std::stringstream command;
        command << "(" << _function << " ";
        auto installArgument = [&command](const std::string& prefix, std::function<void(std::stringstream&)> body, const std::string& suffix) {
            command << " " << prefix;
            body(command);
            command << suffix << " ";
        };
        auto installAddress = [installArgument](const std::string& type, void* value) {
            installArgument(type + "<0x", [value](std::stringstream& stream) { stream << std::hex << value; }, ">");
        };
        for (auto args = _ref.argList; args; args = args->nextArg) {
            auto aType = static_cast<DataObjectType>(args->type);
            switch (aType) {
                case DataObjectType::String:
                    installArgument("\"", [args](std::stringstream& stream) { stream << DOPToString(args); }, "\"");
                    break;
                case DataObjectType::Symbol:
                    installArgument("", [args](std::stringstream& stream) { stream << DOPToString(args); }, "");
                    break;
                case DataObjectType::InstanceName:
                    installArgument("[", [args](std::stringstream& stream) { stream << DOPToString(args); }, "]");
                    break;
                case DataObjectType::Integer:
                    installArgument("", [args](std::stringstream& stream) { stream << DOPToInteger(args); }, "");
                    break;
                case DataObjectType::Float:
                    installArgument("", [args](std::stringstream& stream) { stream << DOPToDouble(args); }, "");
                    break;
                case DataObjectType::InstanceAddress:
                    installAddress("InstanceAddress", args->value);
                    break;
                case DataObjectType::ExternalAddress:
                    installAddress("ExternalAddress", args->value);
                    break;
                case DataObjectType::Multifield:
                    /// @todo When we need to do multifield walking in a
                    ///  function then modify this case to perform the walking
                    installAddress("MULTIFIELD", args->value);
                    break;
                default:
                    installAddress("UNKNOWN_TYPE", args->value);
                    break;
            }
        }
        command << ")";
        auto str = command.str();
        std::stringstream ss;
        ss << "ERROR: invocation of " << str << " yielded an error!";
        auto tmp = ss.str();
        throw Problem(tmp);
    }
}

void
FunctionBuilder::addArgument(int32 value)
{
    installArgument(INTEGER, env->addNumber(value));
}

void
FunctionBuilder::addArgument(std::function<void(FunctionBuilder*)> fn)
{
    fn(this);
}

// end FunctionBuilder stuff


template<typename T>
void
populateStringContainer(void* env, DataObjectPtr dobj, T& container)
{
    int end = GetDOEnd(*dobj),
        begin = GetDOBegin(*dobj);
    void* multifield = GetValue(*dobj);
    for (int i = begin; i <= end; ++i) {
        container.emplace_back(ValueToString(EnvGetMFValue(env, multifield, i)));
    }
}

void
populateStringContainer(void* env, DataObjectPtr dobj, std::stringstream& container)
{
    int end = GetDOEnd(*dobj),
        begin = GetDOBegin(*dobj);
    void* multifield = GetValue(*dobj);
    for (int i = begin; i <= end; ++i) {
        std::string temp = ValueToString(EnvGetMFValue(env, multifield, i));
        container << temp << "\n";
    }
}

void
extractData(Environment* env, DataObjectPtr dobj, std::list<std::string>& list)
{
    populateStringContainer(env->getRawEnvironment(), dobj, list);
}

void
extractData(Environment* env, DataObjectPtr dobj, std::vector<std::string>& list)
{
    populateStringContainer(env->getRawEnvironment(), dobj, list);
}

void
extractData(Environment* env, DataObjectPtr dobj, std::stringstream& value)
{
    populateStringContainer(env->getRawEnvironment(), dobj, value);
}

void
extractData(Environment* env, DataObjectPtr dobj, int32& value)
{
    value = EnvDOPToInteger(env->getRawEnvironment(), dobj);
}
void
extractData(Environment* env, DataObjectPtr dobj, uint32& value)
{
    value = EnvDOPToInteger(env->getRawEnvironment(), dobj);
}

void
extractData(Environment* env, DataObjectPtr dobj, uint64& value)
{
    //The EnvDOPToLong macro returns an integer of types long long (according
    //to the integer hash node struct).
    value = (uint64)EnvDOPToLong(env->getRawEnvironment(), dobj);
}
void
extractData(Environment* env, DataObjectPtr dobj, int64& value)
{
    value = EnvDOPToLong(env->getRawEnvironment(), dobj);
}
void
extractData(Environment* env, DataObjectPtr dobj, float& value)
{
    value = EnvDOPToFloat(env->getRawEnvironment(), dobj);
}
void
extractData(Environment* env, DataObjectPtr dobj, double& value)
{
    value = EnvDOPToDouble(env->getRawEnvironment(), dobj);
}
void
extractData(Environment* env, DataObjectPtr dobj, std::string& value)
{
    value = EnvDOPToString(env->getRawEnvironment(), dobj);
}

void
extractData(Environment* env, DataObjectPtr dobj, bool& value) {
    value = (dobj->value != (void*)(EnvFalseSymbol(env->getRawEnvironment()))) && (dobj->type == SYMBOL);
}

// begin MultifieldBuilder stuff

MultifieldBuilder::MultifieldBuilder(Environment* env, int32 size) : MultifieldBuilder(env->createMultifield(size)) { }

void
MultifieldBuilder::setField(DataObjectType type, int index, void* value) {
    SetMFType(_rawMultifield, index, static_cast<int16>(type));
    SetMFValue(_rawMultifield, index, value);
}


void
SetDataObjectType(DataObjectPtr ptr, DataObjectType type)
{
    SetpType(ptr, static_cast<unsigned short>(type));
}

void
SetDataObjectValue(DataObjectPtr ptr, void* value)
{
    SetpValue(ptr, value);
}


} // end namespace Neutron
