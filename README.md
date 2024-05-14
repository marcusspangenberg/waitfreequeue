[![main](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)](https://github.com/marcusspangenberg/waitfreequeue/actions/workflows/cmake-multi-platform.yml)

# waitfreequeue

Single header, wait-free queues for C++.
* mpsc_queue.h - Multiple producer, single consumer queue
* spsc_queue.h - Single producer, single consumer queue with queryable size

Both queue types are currently being used in production systems, handling low-latency network packet delivery.

# Requirements
* C++17
* Tested on Linux (clang, gcc) x64, OSX arm64 (clang) and Windows x64 (vs2022)

# Usage

## mpsc_queue

```
#include "waitfreequeue/mpsc_queue.h"


// T is the type of the elements in the queue.
// S is the maximum number of elements in the queue. S must be a power of 2.
waitfree::mpsc_queue<T, S> queue;


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
waitfree::mpsc_queue<ElementType, 16> queue;
queue.push(1, "some string"); 


// Pop an item from the queue.
// Returns false if the queue is empty, otherwise true. The item output parameter is only valid if the function returns 
// true. Not thread safe with regards to other pop operations, thread safe with regards to push operations.
// Element values will be moved if possible, otherwise copied. If elements are not trivially destructible, the element
// destructor will be called after copy.

T element;
if (queue.pop(element)) {
    // element is valid
} else {
    // queue is empty
}


// Checks if the queue is empty.
// Returns true if the queue is empty, otherwise false.
// Not thread safe with regards to pop operations, thread safe with regards to push operations.

if (queue.empty()) {
   // queue is empty
}
```

## spsc_queue

```
#include "waitfreequeue/spsc_queue.h"


// T is the type of the elements in the queue.
// S is the maximum number of elements in the queue. S must be a power of 2.
waitfree::spsc_queue<T, S> queue;


// Push an item to the queue.
// Will assert if the queue is full if asserts are enabled,
// otherwise the behaviour is undefined. The queue should be dimensioned so that this never happens.
// Not thread safe with regards to other push operations. Thread safe with regards to pop operations.
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
waitfree::spsc_queue<ElementType, 16> queue;
queue.push(1, "some string"); 


// Pop an item from the queue.
// Returns false if the queue is empty, otherwise true. The item output parameter is only valid if the function returns 
// true. Not thread safe with regards to other pop operations. Thread safe with regards to push operations.
// Element values will be moved if possible, otherwise copied. If elements are not trivially destructible, the element
// destructor will be called after copy.

T element;
if (queue.pop(element)) {
    // element is valid
} else {
    // queue is empty
}


// Get the current size of the queue.
// Thread safe with regards to push and pop operations.

const size_t size = queue.size();
```

# License
MIT License
