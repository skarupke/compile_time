//#define USE_BOOST_AWAIT
#ifdef USE_BOOST_AWAIT
#	include "boost_await.h"
#	define async(...) boost::async(__VA_ARGS__)
#else
#	include "await.h"
#	define asynchronous(...) resumable(__VA_ARGS__)
#	define async(...) custom_async(__VA_ARGS__)
#endif
#include <chrono>
#include <thread>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <vector>

auto finished = false;

void reschedule()
{
	std::this_thread::sleep_for(std::chrono::milliseconds( rand() % 2000 ));
}

// ___________________________________________________________ //

void async_user_handler();

#include <gtest/gtest.h>
TEST(boost_await, main)
{
	srand(time(0));

	// Custom scheduling is not required - can be integrated
	// to other systems transparently
#ifdef USE_BOOST_AWAIT
	main_tasks.push(
#else
	await_tasks_to_finish.add(
#endif
				[]
	{
		asynchronous([]
		{
			return async_user_handler(),
				   finished = true;
		});
	});

#ifdef USE_BOOST_AWAIT
	Task task;
	while(!finished)
	{
		main_tasks.pop(task);
		task();
	}
#else
	while (!finished) await_tasks_to_finish.run_single_task_blocking();
#endif
}

// __________________________________________________________________ //

int bar(int i)
{
	// await is not limited by "one level" as in C#
	auto result = await async([i]{ return reschedule(), i*100; });
	return result + i*10;
}

int foo(int i)
{
	std::cout << i << ":\tbegin" << std::endl;
	std::cout << await async([i]{ return reschedule(), i*10; }) << ":\tbody" << std::endl;
	std::cout << bar(i) << ":\tend" << std::endl;
	return i*1000;
}

void async_user_handler()
{
#ifdef USE_BOOST_AWAIT
	typedef boost::future<int> future_type;
#else
	typedef then_future<int> future_type;
#endif
	std::vector<future_type> fs;

	for(auto i=0; i!=5; ++i)
		fs.push_back( asynchronous([i]{ return foo(i+1); }) );

	for(auto && f : fs)
		std::cout << await f << ":\tafter end" << std::endl;
}
