Spreadsheet calculation Demo
============================

Build and run with

    g++ -x c++ -std=gnu++17 -O2 -Wall -pedantic -pthread task.cxx
    ./a.out input.txt >output.txt

To use [thread sanitizer][1], build with

    clang++ -x c++ -std=gnu++17 -O1 -pthread -fsanitize=thread -g task.cxx


[1]: https://clang.llvm.org/docs/ThreadSanitizer.html


Restrictions
------------

1. Cell IDs must be: [A-Z][0-9]+, the number in range [0..15777215].

2. Every non-empty line is either:
   * `<cellId> = <number>` or
   * `<cellId> = [±] <cellId> [± <cellId>[ ± <cellId>[ ... ]]]`, here `±` is
     either `+` or `-` sign.

3. No error handling or exception handling.


Improvements
------------

1. Consider using lock-free alternatives for producer-consumer queue. But note
   happens-before relations: `CellData::remote_result` set by worker thread
   must become visible to the main thread after this cell is put to
   `ready_queue`.

2. Use more robust thread pool with thread-local task queues.

3. Implement check of `CellData::epoch` so that the calculated result is
   discarded if the epoch changed.
