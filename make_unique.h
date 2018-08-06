#pragma once
#include <memory>

#if (__cplusplus < 201402L)
// Definition of make_unique taken from https://herbsutter.com/gotw/_102/
template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}
#else
// Added in C++14
using std::make_unique;
#endif

