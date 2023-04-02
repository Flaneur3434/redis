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

	/* lookup test*/
	HashMap<std::string> hm2 {};
	HNode<std::string> node2 ("cat", nullptr, 1);
	hm2.insert(std::move(node2));
	auto lookup_node = hm2.lookup({"cat", nullptr, 1},
	    [](const HNode<std::string>& n1, const HNode<std::string>& n2){
		    return n1.data == n2.data;
	});

	assert_p("check if lookup node is expected node",
	    lookup_node != nullptr && lookup_node->data == node2.data
	);

	return 0;
}
