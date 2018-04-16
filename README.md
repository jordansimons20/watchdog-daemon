# watchdog-daemon
Multithreaded TCP/IP watchdog daemon that continuously monitors a collection of TCP/IP hosts for availability.

To compile program: 
`$ gcc -c watchdogd.c -o watchdogd.o -Wall -Wextra -std=c99 -pthread`
`$ gcc watchdogd.o -o daemon`

Some notes about properly utilizing the program: 
*The program assumes there is a list of hosts to monitor in `/tmp/watchdog.pid`. Please see attached watchdog.pid for an example.
*The daemon will **NOT** terminate by itself, and will continue to run in the background until manually terminated.
*To terminate the daemon, run `$ kill -USR1 `cat watchdog.pid`` in the `/tmp` directory. 
*To check if the daemon is still running, use `$ ps axu`.

To-do: 
*syslog() is not actually logging messages, however the behavior works as expected when printing to stderr.
*Create makefile for compilation ease. 
