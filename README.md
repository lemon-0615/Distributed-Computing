# Distributed-Computing
ITMO_Distributed Computing Lab

## PA1
Program creates communication system using pipes. Child processes notify about START & DONE events via sending messages.

### Compile instructions:
 clang -std=c99 -Wall -pedantic pa1 *.c

### Run:
`./pa1 -p X`, where <b>X</b> - count of child processes.

## PA2
Program creates communication system using pipes. Child processes notify about START & DONE events via sending messages.

### Compile instructions:
clang -std=c99 -Wall -pedantic *.c -L./lib64 -lruntime -o pa2

For running program you need to define the following environment variables:
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/path/to/dir/with/libruntime.so"
LD_PRELOAD=/full/path/to/libruntime.so

### Run:
` ./pa2 –p 2 10 20 `.


## PA3
Same as PA2. Instead of Physical time here is used Lamport time.

## PA4
Working with critical area as child process useful work.

### Run:
`./pa1 -p X [--mutexl]`, where <b>X</b> - count of child processes, <b>--mutexl</b> - tells program to use Lamport mutex algorithm in critical area
