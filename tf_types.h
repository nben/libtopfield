#ifndef _TF_TYPES_H
#define _TF_TYPES_H

#include <sys/types.h>
#include <stdlib.h>

#ifdef __linux
#include <linux/types.h>
#else
typedef unsigned long long __u64;
typedef unsigned int __u32;
typedef unsigned short __u16;
typedef unsigned char __u8;
#endif

#endif
