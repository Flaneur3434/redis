#include "hashmap.hpp"

int main() {
	HashMap<int> hm {};
	HNode<int> node1 (5, nullptr, 1);

	/* insert test */
	hm.insert(std::move(node1));
	assert_p("check if hashtable size is right", hm.ht1->size == 3);

	/* pop test */
	HNode<int> key (5, nullptr, 1);
	auto popped_node = hm.pop(key, [](const HNode<int>& n1, const HNode<int>& n2){
		return n1.data == n2.data;
	});
	assert_p("check if popped node is expected", popped_node.data == 5);

	return 0;
}
