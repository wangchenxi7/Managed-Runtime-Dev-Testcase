#include "jvmti.h"

// rescribo
#include <class_file.hpp>

#include <code.hpp>
#include <constant_pool.hpp>
#include <constant_pool_entry.hpp>
#include <method.hpp>
#include <methods.hpp>

// C/C++ standard headers
#include <cassert>
#include <cstring> // Remove with conditional for SortBimorphic
#include <list>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "stdint.h"



using namespace project_rescribo;

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved);
void Agent_OnUnload(JavaVM *vm);


void JNICALL ClassFileLoadHook(jvmtiEnv* env,
                               JNIEnv* jni_env,
                               jclass class_being_redefined,
                               jobject loader,
                               const char* name,
                               jobject protection_domain,
                               jint class_data_len,
                               const unsigned char* class_data,
                               jint* new_class_data_len,
                               unsigned char** new_class_data);


// Debug functions

/**
 * The failed corner cases.
 * Skip them.
 * 
 * Parameter : unique_ptr, the smart pointer. It can't be coppied.
 * 
 */
bool function_filtered(std::unique_ptr<Method> & method);
