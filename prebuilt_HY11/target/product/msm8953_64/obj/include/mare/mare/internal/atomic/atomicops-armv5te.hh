// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
// Copyright 2013-2015 Qualcomm Technologies, Inc.
// All rights reserved.
// Confidential and Proprietary – Qualcomm Technologies, Inc.
// --~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~----~--~--~--~--
#pragma once
#include <cstdint>
#include <climits>

extern "C" {

static inline uint64_t
__sync_val_compare_and_swap_8(uint64_t *ptr, uint64_t oldval, uint64_t newval)
{
  uint64_t tmp;

  asm volatile ("@ mare___sync_val_compare_and_swap_8 \n"

                "        ldrd   %[tmp], %H[tmp], [%[ptr]] \n"
                "        cmp    %H[tmp], %H[exp] \n"
                "        it     eq \n"
                "        cmpeq  %[tmp], %[exp] \n"
                "        bne    2f \n"
                "        strd   %[des], %H[des], [%[ptr]] \n"
                "2: # exit \n"
                : [tmp] "=&r" (tmp),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (oldval),
                  [des] "r" (newval)
                : "cc", "memory");
  return tmp;
}

static inline bool
__sync_bool_compare_and_swap_8(uint64_t *ptr, uint64_t oldval, uint64_t newval)
{
  uint64_t tmp;
  size_t failed_write;

  asm volatile ("@ mare___sync_val_compare_and_swap_8 \n"

                "        mov   %[fw], #1 \n"
                "        ldrd   %[tmp], %H[tmp], [%[ptr]] \n"
                "        cmp    %H[tmp], %H[exp] \n"
                "        it     eq \n"
                "        cmpeq  %[tmp], %[exp] \n"
                "        bne    2f \n"
                "        strd   %[des], %H[des], [%[ptr]] \n"
                "        mov   %[fw], #0 \n"
                "2: # exit \n"
                : [tmp] "=&r" (tmp),
                  [fw] "=&r" (failed_write),
                  "=Qo" (*ptr)
                : [ptr] "r" (ptr),
                  [exp] "r" (oldval),
                  [des] "r" (newval)
                : "cc", "memory");
  return !failed_write;
}

}
