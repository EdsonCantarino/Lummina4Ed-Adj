#ifndef queue_h
#define queue_h

#include <iostream>
#include <list>
#include <array>
#include <vector>
#include <iterator>

#include <stdio.h>
#include "queue.h"

template <typename T, std::size_t N = 5>
class Queue {
	using block = std::array<T, N>;
	using iterator = typename block::iterator;

public:
	Queue();
	void push(const T& value);
	void pop();
	T front() const;

private:
	std::list<block> storage;
	iterator first, last;
};

template <typename T, std::size_t N>
Queue<T, N>::Queue() {
	storage.push_back(block {});
	first = std::begin(*std::begin(storage));
	last = first;
}

template <typename T, std::size_t N>
void Queue<T, N>::push(const T& value) {
	if (last == std::end(*std::begin(storage))) {
		std::cout << "requiring new storage" << '\n';
		storage.push_back(block {});
		last = std::begin(*std::begin(storage));
	}
	*last++ = value;
}

template <typename T, std::size_t N>
void Queue<T, N>::pop() {
	if (++first == std::end(*std::begin(storage))) {
		std::cout << "freeing unused memory" << '\n';
		storage.pop_front();
		first = std::begin(*std::begin(storage));
	}
}

template <typename T, std::size_t N>
T Queue<T, N>::front() const {
	return *first;
}

#endif
