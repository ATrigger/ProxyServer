#ifndef _DEBUG_H
#define _DEBUG_H
#undef DEBUG
#include<stdio.h>
#ifdef DEBUG
#define INFO(format) fprintf(stderr,"%s:%d:%s -> " format "\n", __FILE__, __LINE__, __func__)
#define LOG(format, ...) fprintf(stderr,"%s:%d:%s -> " format "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)

#else

#define LOG(...)
#define INFO(...)

#endif
#endif