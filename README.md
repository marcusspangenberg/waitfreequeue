[![main](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml)

# waitfreequeue
Single header, wait-free, multiple producer, single consumer queue for C++.

# Requirements
* C++17
* Tested on Linux (clang, gcc) x64, OSX arm64 (clang) and Windows x64 (vs2022)

# Usage
```
#include "waitfreequeue/WaitFreeMPSCQueue.h"


// SIZE must be a power of 2
WaitFreeMPSCQueue<ELEMENT_TYPE, SIZE> queue;


// push will assert if the queue is full if asserts are enabled,
// otherwise the behaviour is undefined. The queue should be dimensioned so that this 
// never happens.
//
// push is not thread safe with regards to other push operations, but is thread safe
// with regards to pop operations.

ELEEENT_TYPE element;
queue.push(element);


// pop will return false if the queue is empty, 
// the element is only valid if the return value is true
// pop is thread safe with regards to other pop operations and to push operations

ELEMENT_TYPE element;
if (queue.pop(element)) {
    // element is valid
} else {
    // queue is empty
}

```

# License
MIT License
