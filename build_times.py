import os
import subprocess
import time

directory = "num_iterations"
if not os.path.exists(directory):
	os.makedirs(directory)

os.chdir(directory)

prepare_command = 'qmake-qt4 ../compile_time.pro -r -spec unsupported/linux-clang DEFINES+=%s'
clean_command = 'make clean'
build_command = 'make -j4'
define = 'NUM_ITERATIONS=%s'

times = {}

for i in range(10):
	num_iterations = (2 ** (i + 1))
	my_define = define % num_iterations
	subprocess.call((prepare_command % my_define).split())
	subprocess.call(clean_command.split())
	time_before = time.time()
	subprocess.call(build_command.split())
	times[num_iterations] = time.time() - time_before

for k in sorted(times.keys()):
	print k, ": ", times[k]

