import os
import subprocess
import time

directory = "num_iterations"
if not os.path.exists(directory):
	os.makedirs(directory)

os.chdir(directory)

prepare_command = 'qmake-qt4 ../compile_time.pro -r -spec unsupported/linux-clang DEFINES+=%s'
#prepare_command += ' DEFINES+=COMPILE_UNIQUE_PTR'
prepare_command += ' DEFINES+=COMPILE_FLAT_MAP'
prepare_command += ' DEFINES+=BOOST_FLAT_MAP'
clean_command = 'rm main.o'
build_command = 'make -j4'
define = 'NUM_ITERATIONS=%s'

times = {}

test_run = 'COMPILE_UNIQUE_PTR'

for i in range(7):
	num_iterations = (2 ** (i + 1))
	my_define = define % num_iterations
	subprocess.call((prepare_command % my_define).split())
	subprocess.call(clean_command.split())
	time_before = time.time()
	subprocess.call(build_command.split())
	times[num_iterations] = time.time() - time_before

for k in sorted(times.keys()):
	print k, ": ", times[k]

