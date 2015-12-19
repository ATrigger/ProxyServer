//
// Created by kamenev on 19.12.15.
//

#ifndef POLL_EVENT_OUTSTRING_H
#define POLL_EVENT_OUTSTRING_H
#include <string>
struct outstring{
    std::string text;
    size_t pp=0;

    outstring(std::string);
    const char * get();
    size_t size();
    explicit operator bool();
    void operator +=(size_t);
};
#endif //POLL_EVENT_OUTSTRING_H
