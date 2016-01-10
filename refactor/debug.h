#ifndef _DEBUG_H
#define _DEBUG_H
#include<cstdio>
#ifdef DEBUG
#define INFO(format) fprintf(stderr,"(%s)%s:%d:%s -> " format "\n",__TIME__, __FILE__, __LINE__, __func__)
#define LOG(format, ...) fprintf(stderr,"(%s)%s:%d:%s -> " format "\n",__TIME__, __FILE__, __LINE__, __func__, __VA_ARGS__)

#else

#define LOG(...)
#define INFO(...)

#endif
#endif