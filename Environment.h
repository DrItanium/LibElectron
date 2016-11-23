/**
 * @file
 * C++ wrapper functions for building CLIPS environments
 * @copyright
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

#ifndef __LibNeutron_Environment_h__
#define __LibNeutron_Environment_h__
#include <cstdint>
#include <functional>
#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <utility>
#include <map>
#include <typeinfo>
#include <exception>

extern "C" {
    #include "clips.h"
}

namespace Neutron
{

class Problem : public std::exception {
public:
    explicit Problem(const std::string& msg) noexcept : _msg(msg) { }
    virtual ~Problem() noexcept { }

    /**
     * @return a std::string descripting the message
     */
    std::string message() const         { return _msg; }

    virtual const char* what() const noexcept final { return _msg.c_str(); }

    /**
     * Problem objects, (and their derivatives), should not be assigned
     */
    Problem& operator=(const Problem&) = delete;
private:
    std::string _msg;
};

using DataObject = DATA_OBJECT;
using DataObjectPtr = DATA_OBJECT_PTR;

/**
 * A wrapper over the raw macros in CLIPS
 */
enum class DataObjectType : int {
    Float = FLOAT,
    Integer = INTEGER,
    Symbol = SYMBOL,
    String = STRING,
    Multifield = MULTIFIELD,
    ExternalAddress = EXTERNAL_ADDRESS,
    FactAddress = FACT_ADDRESS,
    InstanceAddress = INSTANCE_ADDRESS,
    InstanceName = INSTANCE_NAME,

    IntegerOrFloat = INTEGER_OR_FLOAT,
    SymbolOrString = SYMBOL_OR_STRING,
    InstanceOrInstanceName = INSTANCE_OR_INSTANCE_NAME,
};


void SetDataObjectType(DataObjectPtr ptr, DataObjectType type);
void SetDataObjectValue(DataObjectPtr ptr, void* value);



class Environment;
/**
 * A special function building class that is useful when funcall is evaluating
 * arguments and it shouldn't be.
 */
class FunctionBuilder
{
public:
    FunctionBuilder(Environment* env);
    ~FunctionBuilder();
    void setFunctionReference(const std::string& func);
    /**
     * raw interface to CLIPS, you must call the corresponding CLIPS functions
     * yourself.
     * @param type The CLIPS type (see constant.h)
     * @param value The CLIPS converted value
     */
    void installArgument(uint16_t type, void* value);
    void installArgument(DataObjectType type, void* value);
    /**
     * Install the given string as an argument to the given expression
     * @param chars the chars to make an expression out of
     */
    void addArgument(const char* chars);

    void addArgument(const std::string& str);

    void addArgument(int64_t value);

    void addArgument(int32_t value);

    void addArgument(bool value);

    void addArgument(float value);

    void addArgument(double value);

    template<typename T>
    void addArgument(T* value);
    /**
     * Flatten the contents of the std::list by adding individual elements
     * @param list the list which has the contents to add to the expression
     */
    template<typename T>
    void addArgument(const std::list<T>& list)
    {
        for (auto const &element : list) {
            addArgument(element);
        }
    }

    template<typename T>
    void addArgument(const std::initializer_list<T>& list)
    {
        for (auto const &element : list) {
            addArgument(element);
        }

    }

    template<typename T>
    void addArgument(const std::vector<T>& list)
    {
        for (auto const &element : list) {
            addArgument(element);
        }
    }

    template<typename First, typename ... Args>
    void addArgument(First f, Args ... values)
    {
        addArgument(f);
        addArgument(values...);
    }

    void invoke(DataObjectPtr ret);

    void addArgument(std::function<void(FunctionBuilder*)> fn);

    /**
     * Return a function which operates on a begin and end iterator.
     */
    template<typename I>
    static std::function<void(FunctionBuilder*)> collection(I begin, I end)
    {
        return [begin, end](FunctionBuilder* fb) {
            for (I it = begin; it != end; ++it) {
                fb->addArgument(*it);
            }
        };
    }

    /**
     * Return a function which operates on a container.
     */
    template<typename I>
    static std::function<void(FunctionBuilder*)> collection(const I& i)
    {
        return [&i](FunctionBuilder* fb) {
            for (auto it : i) {
                fb->addArgument(it);
            }
        };
    }

    /**
     * Mark the given thing as a symbol by capturing it and returning it as a
     * std::function.
     * @param thingToConvertToSymbol The thing that we want to convert into a * symbol
     * @return a std::function which loads the given thing into the function * builder
     */
    template<typename T>
    static std::function<void(FunctionBuilder*)> symbol(T thingToConvertToSymbol);

    template<typename T>
    static std::function<void(FunctionBuilder*)> externalAddress(T* thingToWrapAsExternalAddress);

private:
    std::string _function;
    Environment* env;
    FUNCTION_REFERENCE _ref;
    EXPRESSION* curr = nullptr;
    bool functionReferenceSet = false;
};

/**
 * simple wrapper class to make the act of constructing a simple multifield
 * much quicker.
 */
class MultifieldBuilder
{
    public:
        explicit MultifieldBuilder(void* rawMultifield) : _rawMultifield(rawMultifield) { }
        MultifieldBuilder(Environment* env, int32_t size);
        void* getRawMultifield() const { return _rawMultifield; }
        void setField(DataObjectType type, int index, void* value);
    private:
        void* _rawMultifield;
};
template<typename T>
struct ExternalAddressCache
{
    public:
        ExternalAddressCache() = delete;
        ~ExternalAddressCache() = delete;
        static int getExternalAddressId(Environment* env);
        static void registerExternalAddressId(Environment* env, int result);
    private:
        static int getExternalAddressId(void* env);
        static void registerExternalAddressId(void* env, int result);
        static std::map<void*, int> _cache;
};
/// Wrapper class to handle the use of a clips environment
class Environment
{
public:
/**
 * A wrapper over the define functions integer values
 */
enum class UserFunctionReturnType : int {
    ExternalAddress = 'a',
    Boolean = 'b',
    Character = 'c',
    DoubleFloat = 'd',
    SingleFloat = 'f',
    Int64 = 'g',
    Int32 = 'i',
    SymbolStringOrInstanceName = 'j',
    SymbolOrString = 'k',
    LongInt = 'l',
    Multifield = 'm',
    IntegerOrFloat = 'n',
    InstanceName = 'o',
    String = 's',
    Any = 'u',
    Void = 'v',
    Symbol = 'w',
    InstanceAddress = 'x',
};

public:
    /// Construct a new electron environment and initialize it
    Environment();

    /// Construct a temporary wrapper over an already existing environment
    Environment(void* env);

    /// Reclaims the memory of a given electron environment
    virtual ~Environment();

    /// Calls the reset function within CLIPS
    void reset();

    /// Runs the given CLIPS environment (rule execution)
    /// @return The number of rules fired
    int64_t run(int64_t count = -1L);

    /// Loads the given source file into the given clips environment
    void loadFile(const std::string& path);

    /// Return the raw environment pointer, USE ONLY IN CASES WHERE FUNCTIONALITY IS MISSING IN THIS CLASS
    void* getRawEnvironment();

    /// Passes in the raw environment to a given function, usually used to define functions
    void applyToFunction(std::function<void(void*)> fn);

    /// Call a function inside an electron environment
    /// @param functionName the name of the function to be invoked
    /// @param args a string containing space deliminated arguments to be passed to the given function
    /// @param obj the struct which will contain the result of the funcall
    /// @throw Problem calling the given function with the specified args resulted in a runtime error
    void funcall(const std::string& functionName, const std::string& args, DataObjectPtr obj);

    /// Call a function inside an electron environment but the result is not important
    /// @param functionName the name of the function to be invoked
    /// @param args The argument list as a constant string
    /// @throw Problem calling the given function with the specified args resulted in a runtime error
    void funcall(const std::string& functionName, const std::string& args);

    /// Call a function inside an electron environment with no arguments nor caring about the result
    /// @param functionName the name of the function to be invoked
    /// @throw Problem calling the given function with the specified args resulted in a runtime error
    void funcall(const std::string& functionName);

    /// Call a function inside an electron environment with no arguments but caring about the result
    /// @param functionName the name of the function to be invoked
    /// @param obj the struct which will contain the result of the funcall
    /// @throw Problem calling the given function with the specified args resulted in a runtime error
    void funcall(const std::string& functionName, DataObjectPtr obj);

    /// Call a function inside an electron environment and then convert the result to a string
    /// @param functionName the name of the function to be invoked
    /// @param args a string containing space deliminated arguments to be passed to the given function
    /// @param ret the string to store the result in
    /// @throw Problem calling the given function with the specified args resulted in a runtime error
    void funcall(const std::string& functionName, const std::string& args, std::string& ret);

    /// Call a function inside an electron environment and then convert the result to a boolean
    /// @param functionName the name of the function to be invoked
    /// @param args a string containing space deliminated arguments to be passed to the given function
    /// @param ret the bool to store the result in
    /// @throw Problem calling the given function with the specified args resulted in a runtime error
    void funcall(const std::string& functionName, const std::string& args, bool& ret);

    /// Call a function inside an electron environment and then convert the result to a list of strings
    /// @param functionName the name of the function to be invoked
    /// @param args The args for this funcall
    /// @param ret the bool to store the result in
    /// @throw Problem calling the given function with the specified args resulted in a runtime error
    template<typename T>
    void funcall(const std::string& functionName, const std::string& args, T& ret)
    {
        DataObject value;
        funcall(functionName, args, &value);
        extractValue(&value, ret);
    }


    /// Enable watch items in the environment
    void watch(const std::string& targets);

    /// disable watch items in the environment
    void unwatch(const std::string& targets);

    /// Registers the given string into the CLIPS symbol table
    /// @return pointer to the registered symbol
    void* addSymbol(const std::string& symbol);

    /// convert const char* to a symbol in the CLIPS symbol table
    void* addSymbol(const char* symbol);

    /// Registers the given number into the CLIPS symbol table
    /// @return pointer to the registered symbol
    void* addNumber(int32_t number);

    /// Registers the given number into the CLIPS symbol table
    /// @return pointer to the registered symbol
    void* addNumber(int64_t number);

    /// Registers the given number into the CLIPS symbol table
    /// @return pointer to the registered symbol
    void* addNumber(uint32_t number);

    /// Registers the given number into the CLIPS symbol table
    /// @return pointer to the registered symbol
    void* addNumber(double number);

    /// Registers the given number into the CLIPS symbol table
    /// @return pointer to the registered symbol
    void* addNumber(float number);

    template<typename T>
    void* addExternalAddress(T* value)
    {
          return ::EnvAddExternalAddress(env, static_cast<void*>(value), ExternalAddressCache<T>::getExternalAddressId(this));
    }


    /// Construct an instance using the given input string (in make-instance syntax)
    /// @return The internal instance pointer
    /// @throw Problem instance construction error
    void* makeInstance(const std::string& instanceString);

    /// Delete an instance
    /// @param instancePtr the instance to delete
    void unmakeInstance(void* instancePtr);

    /// Set the slot of a given instance
    void setSlot(void* instance, const std::string& slotName, DataObjectPtr data);

    /// Get the value stored in the given instance's slot of the specified name
    /// @param instance the instance pointer
    /// @param slotName the name of the slot to access
    /// @param ret the data_object pointer to store the result in
    void getSlot(void* instance, const std::string& slotName, DataObjectPtr ret);

    /// install an expression into the environment (memory management)
    void installExpression(EXPRESSION* expr);

    /// uninstall the given expression from the environment (memory management)
    void deinstallExpression(EXPRESSION* expr);

    /// evaluate the given expression
    bool evaluateExpression(EXPRESSION* expr, DataObjectPtr ret);

    /// reclaim an expression list
    void reclaimExpressionList(EXPRESSION* expr);

    /// generate a constant expression from the given value and type
    EXPRESSION* generateConstantExpression(uint16_t type, void* value);

    /// find the function associated with the given name
    bool generateFunctionExpression(const std::string& name, FUNCTION_REFERENCE* ref);

    void buildAndExecuteFunction(const std::string& function);
    void buildAndExecuteFunction(const std::string& function, DataObjectPtr ret);
    template<typename T0>
    void buildAndExecuteFunction(const std::string& function, DataObjectPtr ret, T0 arg0)
    {
        FunctionBuilder fb(this);
        fb.setFunctionReference(function);
        fb.addArgument(arg0);
        fb.invoke(ret);
    }
    template<typename T0, typename T1>
    void buildAndExecuteFunction(const std::string& function, DataObjectPtr ret, T0 arg0, T1 arg1)
    {
        FunctionBuilder fb(this);
        fb.setFunctionReference(function);
        fb.addArgument(arg0);
        fb.addArgument(arg1);
        fb.invoke(ret);
    }

    /// Build a function expression and then execute it
    /// @param function the name of the function to execute
    /// @param ret The DataObject pointer to store the result in
    /// @param args The variadic list of arguments useful for execution
    template<typename ... Args>
    void buildAndExecuteFunction(const std::string& function, DataObjectPtr ret, Args ... args)
    {
        FunctionBuilder fb(this);
        fb.setFunctionReference(function);
        fb.addArgument(args...);
        fb.invoke(ret);
    }

    /// Build a function expression, execute it, and convert the results
    /// @param function the name of the function to execute
    /// @param ret The data object to store the result in
    /// @param args The variadic list of arguments useful for execution
    template<typename R, typename ... Args>
    void buildAndExecuteFunction(const std::string& function, R& ret, Args ... args)
    {
        DataObject r;
        buildAndExecuteFunction(function, &r, args...);
        extractValue(&r, ret);
    }
    template<typename ... Args>
    void buildAndExecuteFunction(const std::string& function, std::function<void(Environment*, DataObjectPtr)> ret, Args ... args)
    {
        DataObject r;
        buildAndExecuteFunction(function, &r, args...);
        ret(this, &r);
    }
    /// Extract data out of the given data object using the provided function
    /// @param dobj the data object to extract data from
    /// @param fn the function which will convert the given data object to data
    void extractValue(DataObjectPtr dobj, std::function<void(Environment*, DataObjectPtr)> fn);

    template<typename T>
    void extractValue(DataObjectPtr data, T&& value) {
        extractData(this, data, std::forward<T>(value));
    }

    using ExternalAddressType = struct externalAddressType;

    /**
     * Install an external address type structure into the environment; They
     * act as wrappers over external data structures so they can be used safely
     * inside the the Environment.
     * @param description The external address type to install
     * @return the external address index for this specific environment
     */
    int installExternalAddressType(ExternalAddressType* description);

    template<typename T>
    void registerExternalAddressType(ExternalAddressType* description)
    {
        ExternalAddressCache<T>::registerExternalAddressId(this, installExternalAddressType(description));
    }

    template<typename T>
    int getExternalAddressId()
    {
        return ExternalAddressCache<T>::getExternalAddressId(this);
    }

    bool checkArgument(const std::string& function, int position, int type, DataObjectPtr dataObj);

    bool checkArgument(const std::string& function, int position, DataObjectType type, DataObjectPtr dataObj);

    bool argumentIsExternalAddress(const std::string& function, int position, DataObjectPtr dataObj);

    /// @todo fix this when we upgrade to clips 6.40
    using RawFunction = int(*)(void*);
    bool installUserFunction(const std::string& name, int returnType, RawFunction body, const std::string& actualName, const char* restrictions);

    bool installUserFunction(const std::string& name, UserFunctionReturnType returnType, RawFunction body, const std::string& actualName, const char* restrictions);

    /**
     * Build a new raw mutlfield to operate on!
     * @param size the size of the multifield to construct
     * @return an opaque pointer to the raw multifield itself
     */
    void* createMultifield(int32_t size);

    template<typename T>
    bool externalAddressIsOfType(DataObjectPtr ptr)
    {
       return static_cast<struct externalAddressHashNode*>(ptr->value)->type == getExternalAddressId<T>();
    }

    /**
     * convert the given data object pointer containing an external address to the
     * given template type.
     * @return the casted value
     * @param ptr The pointer to the data object containing the external address
     */
    template<typename T>
    T* fromExternalAddress(DataObjectPtr ptr)
    {
        if (externalAddressIsOfType<T>(ptr)) {
            return static_cast<T*>(DOPToExternalAddress(ptr));
        } else {
            std::stringstream ss;
            ss << "Attempted to cast external address to " << typeid(T).name() << " when it is not tagged as such!";
            auto str = ss.str();
            throw Problem(str);
        }
    }

    /**
     * When in a user function, return the number of arguments passed to the
     * user function.
     * @return The number of arguments which are passed to the user function
     */
    int getArgumentCount() const;

protected:
    // For testing
    Environment(int) : env(nullptr) {};

private:
    bool reclaim = false;
    void* env;
};

/// Extract a multifield of strings out of a given DataObject and place its contents inside a std::list
/// @param dobj The data object to extract the multifield contents from
/// @param list A reference to a std::list<string> to store the multifield contents in
void extractData(Environment* env, DataObjectPtr dobj, std::list<std::string>& list);

/// Extract a multifield of strings out of a given DataObject and place its contents inside a std::vector
/// @param dobj The data object to extract the multifield contents from
/// @param list A pointer to a std::vector<string> to store the multifield contents in
void extractData(Environment* env, DataObjectPtr dobj, std::vector<std::string>& list);

/// Extract the int32_t value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the int32_t value from
/// @param value a reference to an int32_t value to be updated
void extractData(Environment* env, DataObjectPtr dobj, int32_t& value);

/// Extract the uint32_t value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the uint32_t value from
/// @param value a reference to an uint32_t value to be updated
void extractData(Environment* env, DataObjectPtr dobj, uint32_t& value);

/// Extract the uint64_t value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the uint64_t value from
/// @param value a reference to an uint64_t value to be updated
void extractData(Environment* env, DataObjectPtr dobj, int64_t& value);

/// Extract the uint64_t value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the uint64_t value from
/// @param value a reference to an uint64_t value to be updated
void extractData(Environment* env, DataObjectPtr dobj, uint64_t& value);

/// Extract the double value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the double value from
/// @param value a reference to an double value to be updated
void extractData(Environment* env, DataObjectPtr dobj, double& value);

/// Extract the float value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the float value from
/// @param value a reference to an float value to be updated
void extractData(Environment* env, DataObjectPtr dobj, float& value);

/// Extract the string value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the string value from
/// @param value a reference to an string value to be updated
void extractData(Environment* env, DataObjectPtr dobj, std::string& value);

/// Extract the boolean value out of a DataObject and place it in a provided field
/// @param dobj The data object to extract the string value from
/// @param value a reference to an string value to be updated
void extractData(Environment* env, DataObjectPtr dobj, bool& value);

/// Extract the value out of a DataObject and place it in a provided stream
/// @param dobj The data object to extract the string value from
/// @param value a reference to an string stream value to be updated
void extractData(Environment* env, DataObjectPtr dobj, std::stringstream& value);

template<typename T>
std::function<void(FunctionBuilder*)>
FunctionBuilder::symbol(T convertToSymbol)
{
    return [&convertToSymbol](FunctionBuilder* fb) {
        fb->installArgument(SYMBOL, (void*)fb->env->addSymbol(convertToSymbol));
    };
}

template<typename T>
std::function<void(FunctionBuilder*)>
FunctionBuilder::externalAddress(T* thingToWrapAsExternalAddress)
{
    return [thingToWrapAsExternalAddress](FunctionBuilder* fb) {
        fb->installArgument(thingToWrapAsExternalAddress);
    };
}

template<typename T>
void
FunctionBuilder::addArgument(T* externalAddress) {
    installArgument(DataObjectType::ExternalAddress, env->addExternalAddress<T>(externalAddress));
}

template<typename T>
std::map<void*, int> ExternalAddressCache<T>::_cache;

template<typename T>
int
ExternalAddressCache<T>::getExternalAddressId(void* env)
{
    auto found = _cache.find(env);
    if (found != _cache.end()) {
        return found->second;
    } else {
        throw Problem("Attempted to get the external address index of something not registered from using an unregistered environment!");
    }
}

template<typename T>
int
ExternalAddressCache<T>::getExternalAddressId(Environment* env)
{
    return getExternalAddressId(env->getRawEnvironment());
}

template<typename T>
void
ExternalAddressCache<T>::registerExternalAddressId(void* env, int result)
{
    _cache.emplace(env, result);
}

template<typename T>
void
ExternalAddressCache<T>::registerExternalAddressId(Environment* env, int result)
{
    registerExternalAddressId(env->getRawEnvironment(), result);
}

} // namespace Neutron
#endif // end __LibNeutron_Environment_h__
