[![main](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml)

# waitfreequeue
Single header, wait-free, multiple producer, single consumer queue for C++.

# Requirements
* C++17
* Tested on Linux (clang, gcc) x64, OSX arm64 (clang) and Windows x64 (vs2022)

# Usage
```
#include "waitfreequeue/WaitFreeMPSCQueue.h"


WaitFreeMPSCQueue<ELEMENT_TYPE, SIZE> queue;


// Will assert if the queue is full if asserts are enabled,
// otherwise the behaviour is undefined. The queue should be dimensioned so that this never happens.
// Thread safe with regards to other push operations and to pop operations.
ELEEENT_TYPE element;
queue.push(element);


// Returns false if the queue is empty, otherwise true. item is only valid if the function returns true.
// Not thread safe with regards to other pop operations, thread safe with regards to push operations.
ELEMENT_TYPE element;
if (queue.pop(element)) {
    // element is valid
} else {
    // queue is empty
}

```

# License
MIT License
