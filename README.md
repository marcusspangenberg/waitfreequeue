[![main](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml)

# waitfreequeue
Single header, wait-free, multiple producer, single consumer queue for C++.

# Requirements
* C++17
* Tested on Linux (clang, gcc) x64, OSX arm64 (clang) and Windows x64 (vs2022)

# Usage
```
#include "waitfreequeue/WaitFreeMPSCQueue.h"


// T is the type of the elements in the queue.
// S is the maximum number of elements in the queue. S must be a power of 2.
WaitFreeMPSCQueue<T, S> queue;


// Push an item to the queue.
// Will assert if the queue is full if asserts are enabled,
// otherwise the behaviour is undefined. The queue should be dimensioned so that this never happens.
// Thread safe with regards to other push operations and to pop operations.
T element;
queue.push(element);


// Elements can be constructed in place as well
struct ElementType
{
    ElementType(int a, const std::string& b)
    : a_(a), b_(b)
    {}
    
    int a_;
    std::string b_; 
};

ElementType element;
WaitFreeMPSCQueue<ElementType, 16> queue;
queue.push(1, "some string"); 


// Pop an item from the queue.
// Returns false if the queue is empty, otherwise true. item is only valid if the function returns true.
// Not thread safe with regards to other pop operations, thread safe with regards to push operations.
// Element values will be moved if possible, otherwise copied. If elements are not trivially destructible, the element
// destructor will be called after copy.

T element;
if (queue.pop(element)) {
    // element is valid
} else {
    // queue is empty
}

```

# License
MIT License
