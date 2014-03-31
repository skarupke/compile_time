/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
 */

#include "flat_map.hpp"

namespace detail
{
void throw_out_of_range(const char *message)
{
	throw std::out_of_range(message);
}
}

#ifndef DISABLE_GTEST
#include <gtest/gtest.h>
TEST(flat_map, initalizer_list)
{
	flat_map<int, int> foo = { { 5, 7 }, { 4, 3 } };
	ASSERT_EQ(7, foo[5]);
	ASSERT_EQ(3, foo[4]);
	ASSERT_EQ(7, foo.at(5));
	ASSERT_EQ(3, foo.at(4));
	ASSERT_THROW(foo.at(3), std::out_of_range);
	ASSERT_EQ(0, foo[3]);
	ASSERT_EQ((flat_map<int, int>{ { 5, 7 } }), (flat_map<int, int>{ { 5, 7 }, { 5, 7 } }));
}
TEST(flat_map, insert)
{
	flat_map<int, int> foo;
	foo.insert(std::make_pair(5, 4));
	foo.insert(foo.begin(), std::make_pair(3, 4));
	foo.insert(foo.begin(), std::make_pair(3, 4));
	ASSERT_EQ((flat_map<int, int>{ { 5, 4 }, { 3, 4 } }), foo);
	foo.insert(foo.begin(), foo.end());
	ASSERT_EQ((flat_map<int, int>{ { 5, 4 }, { 3, 4 } }), foo);
	std::vector<std::pair<int, int> > bar{ { 6, 7 }, { 8, 7 }, { 1, 7 }, { 8, 7 } };
	foo.insert(bar.begin(), bar.end());
	ASSERT_EQ((flat_map<int, int>{ { 5, 4 }, { 3, 4 }, { 6, 7 }, { 1, 7 }, { 8, 7 } }), foo);
	foo.emplace();
	ASSERT_EQ((flat_map<int, int>{ { 0, 0 }, { 5, 4 }, { 3, 4 }, { 6, 7 }, { 1, 7 }, { 8, 7 } }), foo);
	foo.insert(foo.lower_bound(4), std::make_pair(4, 9));
	ASSERT_EQ((flat_map<int, int>{ { 4, 9 }, { 0, 0 }, { 5, 4 }, { 3, 4 }, { 6, 7 }, { 1, 7 }, { 8, 7 } }), foo);
}
TEST(flat_map, erase)
{
	flat_map<int, int> foo{ { 1, 2 }, { 3, 4 }, { 5, 6 } };
	foo.erase(foo.begin());
	ASSERT_EQ((flat_map<int, int>{ { 3, 4 }, { 5, 6 } }), foo);
	foo.erase(foo.begin() + 1);
	ASSERT_EQ((flat_map<int, int>{ { 3, 4 } }), foo);
	foo.erase(4);
	ASSERT_EQ((flat_map<int, int>{ { 3, 4 } }), foo);
	foo.erase(3);
	ASSERT_TRUE(foo.empty());
}
#include <memory>
TEST(flat_map, destructor)
{
	std::weak_ptr<std::string> weak;
	{
		flat_map<std::unique_ptr<int>, std::shared_ptr<std::string> > foo;
		weak = foo.emplace(std::unique_ptr<int>(new int(5)), std::make_shared<std::string>("foo")).first->second;
		ASSERT_FALSE(weak.expired());
	}
	ASSERT_TRUE(weak.expired());
}
TEST(flat_map, constructor_keep_order)
{
	{
		flat_map<int, int> foo{ { 1, 1 }, { 2, 2 }, { 1, 2 }, { 2, 3 }, { 1, 3 }, { 2, 4 }, { 3, 3 } };
		ASSERT_EQ((flat_map<int, int>{ { 1, 1 }, { 2, 2 }, { 3, 3 } }), foo);
	}
	{
		flat_map<int, int> foo{ { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 2 } };
		ASSERT_EQ(1, foo[1]);
	}
	{
		flat_map<int, int> foo{ { 1, 2 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
								{ 1, 1 } };
		ASSERT_EQ(2, foo[1]);
	}
}

TEST(flat_map, insert_keep_order)
{
	{
		flat_map<int, int> foo;
		foo.insert({ { 1, 1 }, { 2, 2 }, { 1, 2 }, { 2, 3 }, { 1, 3 }, { 2, 4 }, { 3, 3 } });
		ASSERT_EQ((flat_map<int, int>{ { 1, 1 }, { 2, 2 }, { 3, 3 } }), foo);
	}
	{
		std::vector<std::pair<int, int>> pairs = { { 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 2 } };
		std::vector<std::pair<int, int>> pairs_two = { { 1, 2 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 1 }, { 1, 1 }, { 1, 1 }, { 1, 1 },
													{ 1, 1 } };
		flat_map<int, int> foo;
		foo.reserve(pairs.size());
		foo.insert(pairs.begin(), pairs.end());
		ASSERT_EQ(pairs.front().second, foo[1]);
		foo.clear();
		foo.insert(pairs_two.begin(), pairs_two.end());
		ASSERT_EQ(pairs_two.front().second, foo[1]);
	}
}

#ifdef RUN_SLOW_TESTS

TEST(flat_map, insert_many_same)
{
	std::vector<std::pair<int, int>> a;
	a.reserve(100000);
	for (size_t i = 0; i < a.capacity(); ++i)
		a.emplace_back(i % 5, i % 5);
	flat_map<int, int> map;
	map.insert(a.begin(), a.end());
	ASSERT_EQ((flat_map<int, int>{ { 0, 0 }, { 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 } }), map);
}

#endif

TEST(flat_map, insert_a_few_same)
{
	// had a bug where an iterator was assigned from twice
	// use move iterators to make that case obvious
	std::vector<std::pair<std::string, int>> a;
	a.reserve(10);
	for (size_t i = 0; i < a.capacity(); ++i)
		a.emplace_back(std::to_string(i % 5), i % 5);
	a.emplace_back("5", 5);
	a.emplace_back("6", 6);
	flat_map<std::string, int> map;
	map.insert(std::make_move_iterator(a.begin()), std::make_move_iterator(a.end()));
	ASSERT_EQ((flat_map<std::string, int>{ { "0", 0 }, { "1", 1 }, { "2", 2 }, { "3", 3 }, { "4", 4 }, { "5", 5 }, { "6", 6 } }), map);
}

namespace
{
struct NotDefaultConstructibleIntWrapper
{
	NotDefaultConstructibleIntWrapper(int a)
		: a(a)
	{
	}

	bool operator<(const NotDefaultConstructibleIntWrapper & other) const
	{
		return a < other.a;
	}
	bool operator==(const NotDefaultConstructibleIntWrapper & other) const
	{
		return a == other.a;
	}

private:
	int a;
};
}

TEST(flat_map, not_default_constructible)
{
	flat_map<NotDefaultConstructibleIntWrapper, NotDefaultConstructibleIntWrapper> map;
	map.emplace(5, 6);
	map.insert({ { 5, 6 }, { 6, 7 } });
	ASSERT_EQ((flat_map<NotDefaultConstructibleIntWrapper, NotDefaultConstructibleIntWrapper>{ { 5, 6 }, { 6, 7 } }), map);
}

#endif
