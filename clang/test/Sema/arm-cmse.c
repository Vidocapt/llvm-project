// RUN: %clang_cc1 -triple thumbv8m.base-none-eabi -mcmse -verify %s

typedef void (*callback_ns_1t)() __attribute__((cmse_nonsecure_call));
typedef void (*callback_1t)();
typedef void (*callback_ns_2t)() __attribute__((cmse_nonsecure_call));
typedef void (*callback_2t)();

void foo(callback_ns_1t nsfptr, // expected-error{{functions may not be declared with 'cmse_nonsecure_call' attribute}}
         callback_1t fptr) __attribute__((cmse_nonsecure_call))
{
  callback_1t fp1 = nsfptr; // expected-warning{{incompatible function pointer types initializing 'callback_1t'}}
  callback_ns_1t fp2 = fptr; // expected-warning{{incompatible function pointer types initializing 'callback_ns_1t'}}
  callback_2t fp3 = fptr;
  callback_ns_2t fp4 = nsfptr;
}

static void bar() __attribute__((cmse_nonsecure_entry)) // expected-warning{{'cmse_nonsecure_entry' cannot be applied to functions with internal linkage}}
{
}

typedef void nonsecure_fn_t(int) __attribute__((cmse_nonsecure_call));
extern nonsecure_fn_t baz; // expected-error{{functions may not be declared with 'cmse_nonsecure_call' attribute}}

int v0 __attribute__((cmse_nonsecure_call)); // expected-warning {{'cmse_nonsecure_call' only applies to function types; type here is 'int'}}
int v1 __attribute__((cmse_nonsecure_entry)); // expected-warning {{'cmse_nonsecure_entry' attribute only applies to functions}}

void fn0() __attribute__((cmse_nonsecure_entry));
void fn1() __attribute__((cmse_nonsecure_entry(1)));  // expected-error {{'cmse_nonsecure_entry' attribute takes no arguments}}

typedef void (*fn2_t)() __attribute__((cmse_nonsecure_call("abc"))); // expected-error {{'cmse_nonsecure_call' attribute takes no argument}}
