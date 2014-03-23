#include "dunique_ptr.hpp"
#include <memory>

template<typename T, typename D = std::default_delete<T> >
using unique_ptr_type = dunique_ptr<T, D>;

#define CONCAT2(a, b) a ## b
#define CONCAT(a, b) CONCAT2(a, b)

#define USE_A_STRUCT(i)\
struct CONCAT(A, i)\
{\
};\
unique_ptr_type<CONCAT(A, i)> CONCAT(ptr, i)

#define USE_STRUCTS_2(i)\
USE_A_STRUCT(i);\
USE_A_STRUCT(CONCAT(i, 1))
#define USE_STRUCTS_4(i)\
USE_STRUCTS_2(i);\
USE_STRUCTS_2(CONCAT(i, 2))
#define USE_STRUCTS_8(i)\
USE_STRUCTS_4(i);\
USE_STRUCTS_4(CONCAT(i, 4))
#define USE_STRUCTS_16(i)\
USE_STRUCTS_8(i);\
USE_STRUCTS_8(CONCAT(i, 8))
#define USE_STRUCTS_32(i)\
USE_STRUCTS_16(i);\
USE_STRUCTS_16(CONCAT(i, 16))
#define USE_STRUCTS_64(i)\
USE_STRUCTS_32(i);\
USE_STRUCTS_32(CONCAT(i, 32))
#define USE_STRUCTS_128(i)\
USE_STRUCTS_64(i);\
USE_STRUCTS_64(CONCAT(i, 64))
#define USE_STRUCTS_256(i)\
USE_STRUCTS_128(i);\
USE_STRUCTS_128(CONCAT(i, 128))
#define USE_STRUCTS_512(i)\
USE_STRUCTS_256(i);\
USE_STRUCTS_256(CONCAT(i, 256))
#define USE_STRUCTS_1024(i)\
USE_STRUCTS_512(i);\
USE_STRUCTS_512(CONCAT(i, 512))

#define INSTANTIATE2(name) name(0)
#define INSTANTIATE(i) INSTANTIATE2(CONCAT(USE_STRUCTS_, i))

#ifndef NUM_ITERATIONS
#	define NUM_ITERATIONS 1024
#endif

INSTANTIATE(NUM_ITERATIONS);

int main()
{
}

