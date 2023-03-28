#pragma once

#include <memory>
#include <cstdlib>
#include <functional>
#include "util.hpp"

// hashtable node, should be embedded into the payload
struct HNode {
	HNode *next = nullptr;
	uint64_t hcode = 0;
};

// a simple fixed-sized hashtable
class HashTable {
public:
	HashTable(size_t n) noexcept : mask(0), size(0) {
		assert_p("Checking if size is power of 2", n > 0 && ((n - 1) & n) == 0);
		auto temp = (HNode **)calloc(sizeof(HNode *), n);
		table = std::make_unique<HNode **>(temp);
		mask = n - 1;
	}

	~HashTable() noexcept {
		(void)table.release();
	}

	// !! make move & copy constructors

	void insert(HNode&& node) noexcept;
	HNode **lookup(const HNode& key, std::function<bool (const HNode&, const HNode&)> cmp ) const noexcept;
	HNode detach(HNode **from) noexcept;

	// table -> HNode pointer list -> HNode pointer -> HNode
	std::unique_ptr<HNode **> table = nullptr;

	size_t mask = 0;
	size_t size = 0;
};

void HashTable::insert(HNode&& node) noexcept {
	size_t pos = node.hcode & mask;
	HNode *next = *(this->table.get())[pos];
	node.next = next;

	*(this->table.get())[pos] = &node;
	this->size++;
}

// return the address of the parent pointer that owns the target node
HNode **HashTable::lookup(const HNode& key, std::function<bool (const HNode&, const HNode&)> cmp) const noexcept {
	if (table == nullptr) {
		return NULL;
	}

	size_t pos = key.hcode & mask;
	HNode **from = table.get()[pos];

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
HNode HashTable::detach(HNode **from) noexcept {
	HNode *node = *from;
	*from = (*from)->next;
	this->size--;
	return *node;
}

class HashMap {
public:
	HashMap () {
		ht1 = std::make_unique<HashTable>(0);
		ht2 = std::make_unique<HashTable>(0);
		resizing_pos = 0;
	}

	~HashMap () {
		(void)ht1.release();
		(void)ht2.release();
	}

	// !! make move & copy constructors

	HNode *lookup(const HNode& key, std::function<bool (const HNode&, const HNode&)> cmp) noexcept;
	void insert(HNode& node) noexcept;
	HNode pop(const HNode& key, std::function<bool (const HNode&, const HNode&)> cmp) noexcept;
	void help_resize() noexcept;
	void start_resize() noexcept;

	std::unique_ptr<HashTable> ht1;
	std::unique_ptr<HashTable> ht2;

	size_t resizing_pos = 0;
	const size_t k_resizing_work = 128;
	const size_t k_max_load_factor = 8;
};

HNode *HashMap::lookup(const HNode& key, std::function<bool (const HNode&, const HNode&)> cmp) noexcept {
	help_resize();
	HNode **from = this->ht1->lookup(key, cmp);
	if (not from) {
		from = this->ht1->lookup(key, cmp);
	}

	return from ? *from : nullptr;
}


void HashMap::help_resize() noexcept {
	if (this->ht2->table == nullptr) {
		return;
	}

	size_t nwork = 0;
	while(nwork < k_resizing_work && this->ht2->size > 0) {
		// scan for nodes from ht2 and move them to ht1
		HNode **from = this->ht2->table.get()[this->resizing_pos];
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

void HashMap::insert(HNode& node) noexcept {
	if (this->ht1->table == nullptr) {
		this->ht1 = std::make_unique<HashTable>(4);
	}

	this->ht1->insert(std::move(node));

	if (this->ht2->table == nullptr) {
		size_t load_factor = this->ht1->size / (this->ht1->mask + 1);
		if (load_factor >= k_max_load_factor) {
			this->start_resize();
		}
	}

	this->help_resize();
}

HNode HashMap::pop(const HNode& key, std::function<bool (const HNode&, const HNode&)> cmp) noexcept {
	HashMap::help_resize();
	HNode **from = this->ht1->lookup(key, cmp);
	if (from != nullptr) {
		return this->ht1->detach(from);
	}

	from = this->ht2->lookup(key, cmp);
	if (from != nullptr) {
		return this->ht2->detach(from);
	}

	return {};
}

void HashMap::start_resize() noexcept {
	assert_p("check if table is null", this->ht2->table == nullptr);

	this->ht2 = std::move(this->ht1);
	this->ht1 = std::make_unique<HashTable>((this->ht2->mask + 1) * 2);
	this->resizing_pos = 0;
}
