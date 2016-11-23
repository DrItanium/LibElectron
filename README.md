This is a CLIPS 6.30 C++11 wrapper developed at Parasoft for our tools.
Internally we call it Electron but since this release isn't 100% that we've
decided to call this release Neutron :).

While nowhere near as comprehensive as clipsmm, Neutron does take advantage of
C++11 in some pretty neat ways. PLEASE NOTE THAT THIS LIBRARY REQUIRES A
C++ COMPILER THAT FULLY SUPPORTS C++11!

## Using C++ variables directly in function calls

Internally, we pre-process data inside C++ code and then pass the result to
CLIPS for further processing by invoking a function inside the environment with
our data (usually as a string) as an argument. This can get very tricky when
dealing with strings that contain spaces or quote marks because CLIPS'
evaluation will remove them during an EnvFunctionCall. For example:

```
Neutron::Environment env;
env->loadFile("foo.clp");
std::string str("\"foo\""); // ""foo""
std::string second("23"); // 23
bool b = false;
std::stringstream ss;
ss << str << " " << second;
env->funcall("bar", str, b);
```

In this example, the evaluation of str will result in four arguments being
made instead of just two ("" foo "" 23). This is also really time consuming to
do as well.

We've added the ability to directly construct a function expression which
by-passes the evaluation found in EnvFunctionCall and does not require the use
of a std::stringstream or another related concept. Instead, we use C++11's
template parameter packing functionality to build a function expression on the
fly from the list of arguments provided. It also will try to populate the
return value as well.  Here is an example of the entire
thing:

```
Neutron::Environment env;
env->loadFile("foo.clp");
bool b = false;
env->buildAndExecuteFunction("bar", b, "\"foo\"", 23);
```

Now there is no ambiguity of what is going to be happening. The use of the
template parameter pack also allows for an unbounded number of calls.

The design of the function build is such that functions can also be passed as
arguments to perform conversions such as specifying that a string is a symbol
(by default all strings passed in are tagged as STRING).


```
Neutron::Environment env;
env->loadFile("foo.clp");
Neutron::DataObject ret;
env->buildAndExecuteFunction("baz", ret,
                             Neutron::FunctionBuilder::symbol("goo"), // this is a symbol
                             "zoo"); // <- this is a string
```

We use this function all the time to make sure that what our tools originally
processed stay exactly as when sending it to CLIPS.

## Non ad-hoc means of keeping track of registered external addresses per environment

We use the external address mechanism built into CLIPS to provide the ability
to wrap C++ classes in a safe way and use them internally. The only snag we hit
was the need to keep track of the unique ids returned from the environment.
Thus we came up with the external address cache. It is an environment to
integer map which is wrapped in a templated struct. This cache is meant to be
hidden through the use of the environment class. It provides the ability to
find out if a given external address type is of a specified type, get the index
of the external address type as seen by a environment, and cast the given
external address to its corresponding type (if it makes sense).


## Multifield construction

While our implementation is very primitive, we provide a way to construct
multifields. As with everything else, the Environment class is used to
construct one.

## Custom data extraction routines and standard builtins

The CLIPS API is very well written and makes the manipulation of function call
results very easy to extract. We've made it even easier by decoupling the act
of extracting data from the return value from the act of actually calling the
function.

```
Neutron::Environment env;
env->loadFile("foo.clp");
int ret = 0;
// there is a built in extractData routine which will perform the
// EnvDOToInteger call automatically based on what the routine provided is.
env->buildAndExecuteFunction("+", ret, 1, 2);
std::cout << ret << std::endl; // this will be three
```

This is a very simple example but shows that there are built-ins to handle the
conversion of CLIPS types to custom C++ types. By providing a custom
extractData routine, it is possible to provide custom conversion operations as
needed.  For instance:

```
; in clips
(deffunction triple-coords
             (?x ?y ?z)
             (create$ (* ?x 3)
                      (* ?y 3)
                      (* ?z 3)))

; in C++
struct Point3i {
    int x;
    int y;
    int z;
};
namespace Neutron {
    void extractData(Environment* env, DataObjectPtr value, Point3i& point) {
        void* multifield = GetValue(*value);
        // safe to throw exceptions here since this will be invoked _after_ we
        // return from the C code
        if (EnvMFGetLength(env->getRawEnvironment(), multifield) < 3) {
            throw Problem("Must have at least three items in a multifield to perform Point3i cast!");
        } else {
            point.x = ValueToInteger(EnvGetMFValue(env, multifield, 1));
            point.y = ValueToInteger(EnvGetMFValue(env, multifield, 2));
            point.z = ValueToInteger(EnvGetMFValue(env, multifield, 3));
        }
    }
}

// code body later on

Environment env;
env->load("triple.clp");
Point3i ret;
env->buildAndExecuteFunction("triple-coords", ret, 1, 2, 3);
// this will result in the ret variable having 3, 6, 9 in it
```


If you attempt to pass an object without an appropriate extractData routine
then a compilation error will occur!


## Building Environment.cc


This project does not come with a makefile since it is not designed to operate
by itself. However, the code can easily be compiled either standalone or
dropped into a CLIPS 63x installation. If you want to compile it byitself here
is the command line using gcc.


```
g++ -c --std=c++11 -I<path/to/clips/core/if/not/in/the/same/directory/as/the/clips/code> Environment.cc
```

