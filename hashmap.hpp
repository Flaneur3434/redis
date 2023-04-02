#pragma once

#include <memory>
#include <cstdlib>
#include <functional>

#include "util.hpp"

// hashtable node, should be embedded into the payload
template <class T>
class HNode {
public:
	HNode (T&& data, HNode<T> *next, uint64_t hcode) : data(data), next(next), hcode(hcode)  {}
	~HNode() = default;

	// move constructor
	HNode(HNode<T>&& other) = default;
	HNode(const HNode<T>& other) = default;
	HNode& operator= (const HNode<T>& other) = default;

	T data;
	HNode<T> *next = nullptr;
	uint64_t hcode = 0;
};

// a simple fixed-sized hashtable
template <class T>
class HashTable {
public:
	HashTable(size_t n) noexcept : mask(0), size(n) {
		assert_p("Checking if size is power of 2", n > 0 && ((n - 1) & n) == 0);
		auto temp = (HNode<T> **)calloc(sizeof(HNode<T> *), n);
		table = std::make_unique<HNode<T> **>(temp);
		mask = n - 1;
	}

	HashTable() noexcept : mask(0), size(0) {
		table = nullptr;
	}

	~HashTable() noexcept {
		(void)table.release();
	}

	// !! make move & copy constructors

	void insert(HNode<T>&& node) noexcept;
	HNode<T> **lookup(const HNode<T>& key, std::function<bool (const HNode<T>&, const HNode<T>&)> cmp ) const noexcept;
	HNode<T> detach(HNode<T> **from) noexcept;

	// table -> HNode<T> pointer list -> HNode<T> pointer -> HNode<T>
	std::unique_ptr<HNode<T> **> table = nullptr;

	size_t mask = 0;
	size_t size = 0;
};

template <class T>
void HashTable<T>::insert(HNode<T>&& node) noexcept {
	size_t pos = node.hcode & mask;
	HNode<T> *next = (*this->table.get())[pos];

	node.next = next;
	(*this->table.get())[pos] = &node;

	this->size++;
}

// return the address of the parent pointer that owns the target node
template <class T>
HNode<T> **HashTable<T>::lookup(const HNode<T>& key, std::function<bool (const HNode<T>&, const HNode<T>&)> cmp) const noexcept {
	if (table == nullptr) {
		return NULL;
	}

	size_t pos = key.hcode & mask;
	HNode<T> **from = &((*table.get())[pos]);

	// traverse table
	while (*from) {
		if (cmp(**from, key)) {
			return from;
		}
		from = &(*from)->next;
	}

	return nullptr;
}

// remove a node from the chain
template <class T>
HNode<T> HashTable<T>::detach(HNode<T> **from) noexcept {
	HNode<T> *node = *from;
	*from = (*from)->next;
	this->size--;
	return *node;
}

template <class T>
class HashMap {
public:
	HashMap () {
		ht1 = std::make_unique<HashTable<T>>(2);
		ht2 = std::make_unique<HashTable<T>>();
		resizing_pos = 0;
	}

	~HashMap () {
		(void)ht1.release();
		(void)ht2.release();
	}

	// !! make move & copy constructors

	HNode<T> lookup(const HNode<T>& key, std::function<bool (const HNode<T>&, const HNode<T>&)> cmp) noexcept;
	void insert(HNode<T>&& node) noexcept;
	HNode<T> pop(const HNode<T>& key, std::function<bool (const HNode<T>&, const HNode<T>&)> cmp) noexcept;
	void help_resize() noexcept;
	void start_resize() noexcept;

	std::unique_ptr<HashTable<T>> ht1;
	std::unique_ptr<HashTable<T>> ht2;

	size_t resizing_pos = 0;
	const size_t k_resizing_work = 128;
	const size_t k_max_load_factor = 8;
};

template <class T>
HNode<T> HashMap<T>::lookup(const HNode<T>& key, std::function<bool (const HNode<T>&, const HNode<T>&)> cmp) noexcept {
	help_resize();
	HNode<T> **from = this->ht1->lookup(key, cmp);
	if (from == nullptr) {
		from = this->ht2->lookup(key, cmp);
	}

	return from ? **from : HNode<T>{};
}

template <class T>
void HashMap<T>::help_resize() noexcept {
	if (this->ht2->table.get() == nullptr) {
		return;
	}

	size_t nwork = 0;
	while(nwork < k_resizing_work && this->ht2->size > 0) {
		// scan for nodes from ht2 and move them to ht1
		HNode<T> **from = this->ht2->table.get()[this->resizing_pos];
		if (*from == nullptr) {
			++this->resizing_pos;
			continue;
		}

		this->ht1->insert(this->ht2->detach(from));
		++nwork;
	}

	if (this->ht2->size == 0) {
		(void)this->ht2.release();
	}
}

template <class T>
void HashMap<T>::insert(HNode<T>&& node) noexcept {
	if (this->ht1->table.get() == nullptr) {
		this->ht1 = std::make_unique<HashTable<T>>(4);
	}

	this->ht1->insert(std::move(node));

	if (this->ht2->table.get() == nullptr) {
		size_t load_factor = this->ht1->size / (this->ht1->mask + 1);
		if (load_factor >= k_max_load_factor) {
			this->start_resize();
		}
	}

	this->help_resize();
}

template <class T>
HNode<T> HashMap<T>::pop(const HNode<T>& key, std::function<bool (const HNode<T>&, const HNode<T>&)> cmp) noexcept {
	HashMap<T>::help_resize();
	HNode<T> **from = this->ht1->lookup(key, cmp);
	if (from != nullptr) {
		return this->ht1->detach(from);
	}

	from = this->ht2->lookup(key, cmp);
	if (from != nullptr) {
		return this->ht2->detach(from);
	}

	return HNode<T>(T{}, nullptr, 0);
}

template <class T>
void HashMap<T>::start_resize() noexcept {
	assert_p("check if table is null", this->ht2->table.get() == nullptr);

	this->ht2 = std::move(this->ht1);
	this->ht1 = std::make_unique<HashTable<T>>((this->ht2->mask + 1) * 2);
	this->resizing_pos = 0;
}
