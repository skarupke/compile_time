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

#include <memory>

// behaves pretty much like std::unique_ptr but compiles
// faster than the libstdc++ version. this doesn't
// support a custom deleter though
template<typename T, typename Delete = std::default_delete<T> >
struct dunique_ptr;

template<typename T>
struct dunique_ptr<T, std::default_delete<T> >
{
	dunique_ptr()
		: ptr(nullptr)
	{
	}
	explicit dunique_ptr(T * ptr)
		: ptr(ptr)
	{
	}
	dunique_ptr(const dunique_ptr &) = delete;
	dunique_ptr & operator=(const dunique_ptr &) = delete;
	dunique_ptr(dunique_ptr && other)
		: ptr(other.ptr)
	{
		other.ptr = nullptr;
	}
	dunique_ptr & operator=(dunique_ptr && other)
	{
		swap(other);
		return *this;
	}
	~dunique_ptr()
	{
		reset();
	}
	void reset(T * value = nullptr)
	{
		T * to_delete = ptr;
		ptr = value;
		delete to_delete;
	}
	T * release()
	{
		T * result = ptr;
		ptr = nullptr;
		return result;
	}
	explicit operator bool() const
	{
		return bool(ptr);
	}
	T * get() const
	{
		return ptr;
	}
	T & operator*() const
	{
		return *ptr;
	}
	T * operator->() const
	{
		return ptr;
	}
	void swap(dunique_ptr & other)
	{
		std::swap(ptr, other.ptr);
	}

	bool operator==(const dunique_ptr & other) const
	{
		return ptr == other.ptr;
	}
	bool operator!=(const dunique_ptr & other) const
	{
		return !(*this == other);
	}
	bool operator<(const dunique_ptr & other) const
	{
		return ptr < other.ptr;
	}
	bool operator<=(const dunique_ptr & other) const
	{
		return !(other < *this);
	}
	bool operator>(const dunique_ptr & other) const
	{
		return other < *this;
	}
	bool operator>=(const dunique_ptr & other) const
	{
		return !(*this < other);
	}

private:
	T * ptr;
};

template<typename T, typename D>
void swap(dunique_ptr<T, D> & lhs, dunique_ptr<T, D> & rhs)
{
	lhs.swap(rhs);
}
