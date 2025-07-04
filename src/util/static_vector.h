// Static vector library.
//
// A static vector is a vector backed by a fixed-size array. It's essentially
// a vector, but whose underlying storage is statically allocated and does not
// rely on dynamic memory allocation.

#ifndef YAX86_UTIL_STATIC_VECTOR_H
#define YAX86_UTIL_STATIC_VECTOR_H

#include <stddef.h>
#include <stdint.h>

// Header structure at the beginning of a static vector.
typedef struct StaticVectorHeader {
  // Element size in bytes.
  size_t element_size;
  // Maximum number of elements the vector can hold.
  size_t max_length;
  // Number of elements currently in the vector.
  size_t length;
} StaticVectorHeader;

// Define a static vector type with an element type.
#define STATIC_VECTOR_TYPE(name, element_type, max_length_value)        \
  typedef struct name {                                                 \
    StaticVectorHeader header;                                          \
    element_type elements[max_length_value];                            \
  } name;                                                               \
  static void name##Init(name* vector) {                                \
    static const StaticVectorHeader header = {                          \
        .element_size = sizeof(element_type),                           \
        .max_length = (max_length_value),                               \
        .length = 0,                                                    \
    };                                                                  \
    vector->header = header;                                            \
  }                                                                     \
  static size_t name##Length(const name* vector) {                      \
    return vector->header.length;                                       \
  }                                                                     \
  static element_type* name##Get(name* vector, size_t index) {          \
    if (index >= (max_length_value)) {                                  \
      return NULL;                                                      \
    }                                                                   \
    return &(vector->elements[index]);                                  \
  }                                                                     \
  static bool name##Append(name* vector, const element_type* element) { \
    if (vector->header.length >= (max_length_value)) {                  \
      return false;                                                     \
    }                                                                   \
    vector->elements[vector->header.length++] = *element;               \
    return true;                                                        \
  }                                                                     \
  static bool name##Insert(                                             \
      name* vector, size_t index, const element_type* element) {        \
    if (index > vector->header.length ||                                \
        vector->header.length >= (max_length_value)) {                  \
      return false;                                                     \
    }                                                                   \
    for (size_t i = vector->header.length; i > index; --i) {            \
      vector->elements[i] = vector->elements[i - 1];                    \
    }                                                                   \
    vector->elements[index] = *element;                                 \
    ++vector->header.length;                                            \
    return true;                                                        \
  }                                                                     \
  static bool name##Remove(name* vector, size_t index) {                \
    if (index >= vector->header.length) {                               \
      return false;                                                     \
    }                                                                   \
    for (size_t i = index; i < vector->header.length - 1; ++i) {        \
      vector->elements[i] = vector->elements[i + 1];                    \
    }                                                                   \
    --vector->header.length;                                            \
    return true;                                                        \
  }

#endif  // YAX86_UTIL_STATIC_VECTOR_H
