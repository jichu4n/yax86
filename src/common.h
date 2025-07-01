#ifndef YAX86_COMMON_H
#define YAX86_COMMON_H

// Macro that expands to `static` when bundled.
#ifdef YAX86_IMPLEMENTATION
#define YAX86_PRIVATE static
#else
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_COMMON_H
