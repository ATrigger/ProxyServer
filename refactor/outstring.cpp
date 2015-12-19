//
// Created by kamenev on 19.12.15.
//

#include "outstring.h"
outstring::outstring(std::string string):text(string)
{

}
const char *outstring::get()
{
    return &text.c_str()[pp];
}
size_t outstring::size()
{
    return text.length()-pp;
}
outstring::operator bool()
{
    return pp>=text.length();
}
void outstring::operator+=(size_t t)
{
    pp+=t;
}
