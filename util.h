// ==============================================================================
// YAX86 UTIL MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_UTIL_BUNDLE_H
#define YAX86_UTIL_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/util/static_vector.h start
// ==============================================================================

#line 1 "./src/util/static_vector.h"
// Public interface for the util module.
#ifndef YAX86_UTIL_PUBLIC_H
#define YAX86_UTIL_PUBLIC_H

#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Static vector
// ============================================================================

// A static vector is a vector backed by a fixed-size array. It's essentially
// a vector, but whose underlying storage is statically allocated and does not
// rely on dynamic memory allocation.

typedef struct StaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} StaticVectorHeader;

// Define a static vector type with an element type.
#define STATIC_VECTOR_TYPE(name, type, max_length_arg)                    \
  typedef struct name##StaticVector {                                     \
    StaticVectorHeader header;                                            \
    type elements[max_length];                                            \
  } name##StaticVector;                                                   \
  static inline void name##StaticVectorInit(name##StaticVector* vector) { \
    vector->header = {                                                    \
        .element_size = sizeof(type),                                     \
        .max_length = max_length_arg,                                     \
        .length = 0,                                                      \
    };                                                                    \
  }

// Append an element to the static vector. Returns true if successful, or false
// if the vector is full.


#endif  // YAX86_UTIL_PUBLIC_H


// ==============================================================================
// src/util/static_vector.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/util/static_vector.c start
// ==============================================================================

#line 1 "./src/util/static_vector.c"


// ==============================================================================
// src/util/static_vector.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_UTIL_BUNDLE_H

