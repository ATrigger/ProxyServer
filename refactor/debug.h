#ifndef _DEBUG_H
#define _DEBUG_H
#include<cstdio>
#include "utils.h"
#ifdef DEBUG
#define INFO(format) fprintf(stderr,"(%s)%s:%d:%s -> " format "\n",currentTime().c_str(), __FILE__, __LINE__, __func__)
#define LOG(format, ...) fprintf(stderr,"(%s)%s:%d:%s -> " format "\n",currentTime().c_str(), __FILE__, __LINE__, __func__, __VA_ARGS__)

#else

#define LOG(...)
#define INFO(...)

#endif
#endif