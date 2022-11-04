all: OperatingSystem_TaskScheduler

a3: OperatingSystem_TaskScheduler.c
	clang OperatingSystem_TaskScheduler.c -Wall -Wpedantic -Wextra -Werror -o OperatingSystem_TaskScheduler

clean:
	rm -f OperatingSystem_TaskScheduler
