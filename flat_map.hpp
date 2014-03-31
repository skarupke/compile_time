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

#pragma once

#include <vector>
#include <algorithm>
#include <functional>

namespace detail
{
void throw_out_of_range(const char * message);
}

template<typename K, typename V, typename Comp = std::less<K>, typename Allocator = std::allocator<std::pair<K, V> > >
struct flat_map
{
	typedef K key_type;
	typedef V mapped_type;
	typedef std::pair<K, V> value_type;
	typedef Comp key_compare;
	struct value_compare : std::binary_function<value_type, value_type, bool>
	{
		bool operator()(const value_type & lhs, const value_type & rhs) const
		{
			return key_compare()(lhs.first, rhs.first);
		}
	};
	typedef Allocator allocator_type;
	typedef V & reference;
	typedef const V & const_reference;
	typedef typename std::allocator_traits<allocator_type>::pointer pointer;
	typedef typename std::allocator_traits<allocator_type>::const_pointer const_pointer;
	typedef std::vector<value_type, allocator_type> container_type;
	typedef typename container_type::iterator iterator;
	typedef typename container_type::const_iterator const_iterator;
	typedef typename container_type::reverse_iterator reverse_iterator;
	typedef typename container_type::const_reverse_iterator const_reverse_iterator;
	typedef typename container_type::difference_type difference_type;
	typedef typename container_type::size_type size_type;

	flat_map() = default;
	template<typename It>
	flat_map(It begin, It end)
	{
		insert(begin, end);
	}
	flat_map(std::initializer_list<value_type> init)
		: flat_map(init.begin(), init.end())
	{
	}

	iterator				begin()				{	return data.begin();	}
	iterator				end()				{	return data.end();		}
	const_iterator			begin()		const	{	return data.begin();	}
	const_iterator			end()		const	{	return data.end();		}
	const_iterator			cbegin()	const	{	return data.cbegin();	}
	const_iterator			cend()		const	{	return data.cend();		}
	reverse_iterator		rbegin()			{	return data.rbegin();	}
	reverse_iterator		rend()				{	return data.rend();		}
	const_reverse_iterator	rbegin()	const	{	return data.rbegin();	}
	const_reverse_iterator	rend()		const	{	return data.rend();		}
	const_reverse_iterator	crbegin()	const	{	return data.crbegin();	}
	const_reverse_iterator	crend()		const	{	return data.crend();	}

	bool empty() const
	{
		return data.empty();
	}
	size_type size() const
	{
		return data.size();
	}
	size_type max_size() const
	{
		return data.max_size();
	}
	size_type capacity() const
	{
		return data.capacity();
	}
	void reserve(size_type size)
	{
		data.reserve(size);
	}
	void shrink_to_fit()
	{
		data.shrink_to_fit();
	}

	mapped_type & operator[](const key_type & key)
	{
		KeyOrValueCompare comp;
		auto lower = lower_bound(key);
		if (lower == end() || comp(key, *lower)) return data.emplace(lower, key, mapped_type())->second;
		else return lower->second;
	}
	mapped_type & operator[](key_type && key)
	{
		KeyOrValueCompare comp;
		auto lower = lower_bound(key);
		if (lower == end() || comp(key, *lower)) return data.emplace(lower, std::move(key), mapped_type())->second;
		else return lower->second;
	}
	mapped_type & at(const key_type & key)
	{
		auto found = binary_find(begin(), end(), key, KeyOrValueCompare());
		if (found == end()) detail::throw_out_of_range("key passed to 'at' doesn't exist in this map");
		return found->second;
	}
	const mapped_type & at(const key_type & key) const
	{
		auto found = binary_find(begin(), end(), key, KeyOrValueCompare());
		if (found == end()) detail::throw_out_of_range("key passed to 'at' doesn't exist in this map");
		return found->second;
	}
	std::pair<iterator, bool> insert(value_type && value)
	{
		return emplace(std::move(value));
	}
	std::pair<iterator, bool> insert(const value_type & value)
	{
		return emplace(value);
	}
	iterator insert(const_iterator hint, value_type && value)
	{
		return emplace_hint(hint, std::move(value));
	}
	iterator insert(const_iterator hint, const value_type & value)
	{
		return emplace_hint(hint, value);
	}

	template<typename It>
	void insert(It begin, It end)
	{
		// while I am at the capacity just use normal emplace
		for (; begin != end && size() == capacity(); ++begin)
		{
			emplace(*begin);
		}
		if (begin == end) return;
		// if I don't need to increase capacity I can use a more efficient
		// insert method where I just put everything in the same vector
		// and then merge in place. which gives me
		// O(n + m log(m)) where n is the number of elements in the flat_map
		// and m is std::distance(begin, end)
		size_type size_before = data.size();
		try
		{
			for (size_t i = capacity(); i > size_before && begin != end; --i, ++begin)
			{
				data.emplace_back(*begin);
			}
		}
		catch(...)
		{
			// if emplace_back throws an exception, the easiest way to make sure
			// that our invariants are still in place is to resize to the
			// state we were in before
			for (size_t i = data.size(); i > size_before; --i)
			{
				data.pop_back();
			}
			throw;
		}
		value_compare comp;
		auto mid = data.begin() + size_before;
		std::stable_sort(mid, data.end(), comp);
		std::inplace_merge(data.begin(), mid, data.end(), comp);
		data.erase(std::unique(data.begin(), data.end(), std::not2(comp)), data.end());
		// make sure that we inserted at least one element before recursing. otherwise
		// we'd recurse too often if we were to insert the same element many times
		if (data.size() == size_before)
		{
			for (; begin != end; ++begin)
			{
				if (emplace(*begin).second)
				{
					++begin;
					break;
				}
			}
		}
		// insert the remaining elements that didn't fit by calling this function recursively
		// this will recurse log(n) times where n is std::distance(begin, end)
		return insert(begin, end);
	}
	void insert(std::initializer_list<value_type> il)
	{
		insert(il.begin(), il.end());
	}
	iterator erase(iterator it)
	{
		return data.erase(it);
	}
	iterator erase(const_iterator it)
	{
		return erase(iterator_const_cast(it));
	}
	size_type erase(const key_type & key)
	{
		auto found = find(key);
		if (found == end()) return 0;
		erase(found);
		return 1;
	}
	iterator erase(const_iterator first, const_iterator last)
	{
		return data.erase(iterator_const_cast(first), iterator_const_cast(last));
	}
	void swap(flat_map & other)
	{
		data.swap(other.data);
	}
	void clear()
	{
		data.clear();
	}
	template<typename First, typename... Args>
	std::pair<iterator, bool> emplace(First && first, Args &&... args)
	{
		KeyOrValueCompare comp;
		auto lower_bound = std::lower_bound(data.begin(), data.end(), first, comp);
		if (lower_bound == data.end() || comp(first, *lower_bound)) return { data.emplace(lower_bound, std::forward<First>(first), std::forward<Args>(args)...), true };
		else return { lower_bound, false };
	}
	std::pair<iterator, bool> emplace()
	{
		return emplace(value_type());
	}
	template<typename First, typename... Args>
	iterator emplace_hint(const_iterator hint, First && first, Args &&... args)
	{
		KeyOrValueCompare comp;
		if (hint == cend() || comp(first, *hint))
		{
			if (hint == cbegin() || comp(*(hint - 1), first)) return data.emplace(iterator_const_cast(hint), std::forward<First>(first), std::forward<Args>(args)...);
			else return emplace(std::forward<First>(first), std::forward<Args>(args)...).first;
		}
		else if (!comp(*hint, first)) return begin() + (hint - cbegin());
		else return emplace(std::forward<First>(first), std::forward<Args>(args)...).first;
	}
	iterator emplace_hint(const_iterator hint)
	{
		return emplace_hint(hint, value_type());
	}

	key_compare key_comp() const
	{
		return key_compare();
	}
	value_compare value_comp() const
	{
		return value_compare();
	}

	template<typename T>
	iterator find(const T & key)
	{
		return binary_find(begin(), end(), key, KeyOrValueCompare());
	}
	template<typename T>
	const_iterator find(const T & key) const
	{
		return binary_find(begin(), end(), key, KeyOrValueCompare());
	}
	template<typename T>
	size_type count(const T & key) const
	{
		return std::binary_search(begin(), end(), key, KeyOrValueCompare()) ? 1 : 0;
	}
	template<typename T>
	iterator lower_bound(const T & key)
	{
		return std::lower_bound(begin(), end(), key, KeyOrValueCompare());
	}
	template<typename T>
	const_iterator lower_bound(const T & key) const
	{
		return std::lower_bound(begin(), end(), key, KeyOrValueCompare());
	}
	template<typename T>
	iterator upper_bound(const T & key)
	{
		return std::upper_bound(begin(), end(), key, KeyOrValueCompare());
	}
	template<typename T>
	const_iterator upper_bound(const T & key) const
	{
		return std::upper_bound(begin(), end(), key, KeyOrValueCompare());
	}
	template<typename T>
	std::pair<iterator, iterator> equal_range(const T & key)
	{
		return std::equal_range(begin(), end(), key, KeyOrValueCompare());
	}
	template<typename T>
	std::pair<const_iterator, const_iterator> equal_range(const T & key) const
	{
		return std::equal_range(begin(), end(), key, KeyOrValueCompare());
	}
	allocator_type get_allocator() const
	{
		return data.get_allocator();
	}

	bool operator==(const flat_map & other) const
	{
		return data == other.data;
	}
	bool operator!=(const flat_map & other) const
	{
		return !(*this == other);
	}
	bool operator<(const flat_map & other) const
	{
		return data < other.data;
	}
	bool operator>(const flat_map & other) const
	{
		return other < *this;
	}
	bool operator<=(const flat_map & other) const
	{
		return !(other < *this);
	}
	bool operator>=(const flat_map & other) const
	{
		return !(*this < other);
	}

private:
	container_type data;

	iterator iterator_const_cast(const_iterator it)
	{
		return begin() + (it - cbegin());
	}

	struct KeyOrValueCompare
	{
		bool operator()(const key_type & lhs, const key_type & rhs) const
		{
			return key_compare()(lhs, rhs);
		}
		bool operator()(const key_type & lhs, const value_type & rhs) const
		{
			return key_compare()(lhs, rhs.first);
		}
		template<typename T>
		bool operator()(const key_type & lhs, const T & rhs) const
		{
			return key_compare()(lhs, rhs);
		}
		template<typename T>
		bool operator()(const T & lhs, const key_type & rhs) const
		{
			return key_compare()(lhs, rhs);
		}
		bool operator()(const value_type & lhs, const key_type & rhs) const
		{
			return key_compare()(lhs.first, rhs);
		}
		bool operator()(const value_type & lhs, const value_type & rhs) const
		{
			return key_compare()(lhs.first, rhs.first);
		}
		template<typename T>
		bool operator()(const value_type & lhs, const T & rhs) const
		{
			return key_compare()(lhs.first, rhs);
		}
		template<typename T>
		bool operator()(const T & lhs, const value_type & rhs) const
		{
			return key_compare()(lhs, rhs.first);
		}
	};

	// like std::binary_search, but returns the iterator to the element
	// if it was found, and returns end otherwise
	template<typename It, typename T, typename Compare>
	static It binary_find(It begin, It end, const T & value, const Compare & cmp)
	{
		auto lower_bound = std::lower_bound(begin, end, value, cmp);
		if (lower_bound == end || cmp(value, *lower_bound)) return end;
		else return lower_bound;
	}
};

template<typename K, typename V, typename C, typename A>
void swap(flat_map<K, V, C, A> & lhs, flat_map<K, V, C, A> & rhs)
{
	lhs.swap(rhs);
}
