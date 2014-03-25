#define CONCAT2(a, b) a ## b
#define CONCAT(a, b) CONCAT2(a, b)

#define USE_STRUCTS_2(i)\
USE_A_STRUCT(i);\
USE_A_STRUCT(CONCAT(i, 1))
#define USE_STRUCTS_4(i)\
USE_STRUCTS_2(i);\
USE_STRUCTS_2(CONCAT(i, 2))
#define USE_STRUCTS_8(i)\
USE_STRUCTS_4(i);\
USE_STRUCTS_4(CONCAT(i, 3))
#define USE_STRUCTS_16(i)\
USE_STRUCTS_8(i);\
USE_STRUCTS_8(CONCAT(i, 4))
#define USE_STRUCTS_32(i)\
USE_STRUCTS_16(i);\
USE_STRUCTS_16(CONCAT(i, 5))
#define USE_STRUCTS_64(i)\
USE_STRUCTS_32(i);\
USE_STRUCTS_32(CONCAT(i, 6))
#define USE_STRUCTS_128(i)\
USE_STRUCTS_64(i);\
USE_STRUCTS_64(CONCAT(i, 7))
#define USE_STRUCTS_256(i)\
USE_STRUCTS_128(i);\
USE_STRUCTS_128(CONCAT(i, 8))
#define USE_STRUCTS_512(i)\
USE_STRUCTS_256(i);\
USE_STRUCTS_256(CONCAT(i, 9))
#define USE_STRUCTS_1024(i)\
USE_STRUCTS_512(i);\
USE_STRUCTS_512(CONCAT(i, 0))

#define INSTANTIATE2(name) name(0)
#define INSTANTIATE(i) INSTANTIATE2(CONCAT(USE_STRUCTS_, i))

#ifndef NUM_ITERATIONS
#	define NUM_ITERATIONS 1024
#endif

#ifdef COMPILE_UNIQUE_PTR
#include "dunique_ptr.hpp"
#include <memory>

template<typename T, typename D = std::default_delete<T> >
using unique_ptr_type = dunique_ptr<T, D>;

#	define USE_A_STRUCT(i)\
struct CONCAT(A, i)\
{\
};\
unique_ptr_type<CONCAT(A, i)> CONCAT(ptr, i)
INSTANTIATE(NUM_ITERATIONS);
#endif

#ifdef COMPILE_FLAT_MAP
#	ifdef BOOST_FLAT_MAP
#		include <boost/container/flat_map.hpp>
template<typename K, typename V, typename C = std::less<K>, typename A = std::allocator<std::pair<K, V> > >
using flat_map_type = boost::container::flat_map<K, V, C, A>;
#	else
#		include "flat_map.hpp"
template<typename K, typename V, typename C = std::less<K>, typename A = std::allocator<std::pair<K, V> > >
using flat_map_type = flat_map<K, V, C, A>;
#	endif
#	define USE_A_STRUCT(i)\
struct CONCAT(A, i)\
{\
};\
flat_map_type<int, CONCAT(A, i)> CONCAT(foo, i)()\
{\
	flat_map_type<int, CONCAT(A, i)> map;\
	map.emplace();\
	map.erase(0);\
	return map;\
}\
flat_map_type<int, CONCAT(A, i)> CONCAT(static, i) = CONCAT(foo, i)()
INSTANTIATE(NUM_ITERATIONS);
#endif

#include <gtest/gtest.h>
int main(int argc, char * argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
