#ifndef YAX86_UTIL_COMMON_H
#define YAX86_UTIL_COMMON_H

// Macro that expands to `static` when bundled. Use for variables and functions
// that need to be visible to other files within the same module, but not
// publicly to users of the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

// Macro to mark a function or parameter as unused.
#if defined(__GNUC__) || defined(__clang__)
#define YAX86_UNUSED __attribute__((unused))
#else
#define YAX86_UNUSED
#endif  // defined(__GNUC__) || defined(__clang__)

#endif  // YAX86_UTIL_COMMON_H
