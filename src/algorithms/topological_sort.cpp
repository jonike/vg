#include "topological_sort.hpp"

namespace vg {
namespace algorithms {

using namespace std;

vector<handle_t> head_nodes(const HandleGraph* g) {
    vector<handle_t> to_return;
    g->for_each_handle([&](const handle_t& found) {
        // For each (locally forward) node
        
        bool no_left_edges = true;
        g->follow_edges(found, true, [&](const handle_t& ignored) {
            // We found a left edge!
            no_left_edges = false;
            // We only need one
            return false;
        });
        
        if (no_left_edges) {
            to_return.push_back(found);
        }
    });
    
    return to_return;
    
}

vector<handle_t> tail_nodes(const HandleGraph* g) {
    vector<handle_t> to_return;
    g->for_each_handle([&](const handle_t& found) {
        // For each (locally forward) node
        
        bool no_right_edges = true;
        g->follow_edges(found, false, [&](const handle_t& ignored) {
            // We found a right edge!
            no_right_edges = false;
            // We only need one
            return false;
        });
        
        if (no_right_edges) {
            to_return.push_back(found);
        }
    });
    
    return to_return;
    
}

vector<handle_t> topological_sort(const HandleGraph* g) {
    
    // Make a vector to hold the ordered and oriented nodes.
    vector<handle_t> sorted;
    sorted.reserve(g->node_size());
    
    // Instead of actually removing edges, we add them to this set of masked edges.
    unordered_set<pair<handle_t, handle_t>> masked_edges;
    
    // This (s) is our set of oriented nodes.
    // using a map instead of a set ensures a stable sort across different systems
    map<id_t, handle_t> s;

    // We find the head and tails, if there are any
    vector<handle_t> heads{head_nodes(g)};
    // No need to fetch the tails since we don't use them

    
    // Maps from node ID to first orientation we suggested for it.
    map<id_t, handle_t> seeds;
    
    
    for(handle_t& head : heads) {
        // Dump all the heads into the oriented set, rather than having them as
        // seeds. We will only go for cycle-breaking seeds when we run out of
        // heads. This is bad for contiguity/ordering consistency in cyclic
        // graphs and reversing graphs, but makes sure we work out to just
        // topological sort on DAGs. It mimics the effect we used to get when we
        // joined all the head nodes to a new root head node and seeded that. We
        // ignore tails since we only orient right from nodes we pick.
        s[g->get_id(head)] = head;
    }

    // We will use an ordered map handles by ID for nodes we have not visited
    // yet. This ensures a consistent sort order across systems.
    map<id_t, handle_t> unvisited;
    g->for_each_handle([&](const handle_t& found) {
        if (!s.count(g->get_id(found))) {
            // Only nodes that aren't yet in s are unvisited.
            // Nodes in s are visited but just need to be added tot he ordering.
            unvisited.emplace(g->get_id(found), found);
        }
    });

    while(!unvisited.empty() || !s.empty()) {

        // Put something in s. First go through seeds until we can find one
        // that's not already oriented.
        while(s.empty() && !seeds.empty()) {
            // Look at the first seed
            auto first_seed = (*seeds.begin()).second;

            if(unvisited.count(g->get_id(first_seed))) {
                // We have an unvisited seed. Use it
#ifdef debug
#pragma omp critical (cerr)
                cerr << "Starting from seed " << g->get_id(first_seed) << " orientation " << g->get_is_reverse(first_seed) << endl;
#endif

                s[g->get_id(first_seed)] = first_seed;
                unvisited.erase(g->get_id(first_seed));
            }
            // Whether we used the seed or not, don't keep it around
            seeds.erase(seeds.begin());
        }

        if(s.empty()) {
            // If we couldn't find a seed, just grab any old node.
            // Since map order is stable across systems, we can take the first node by id and put it locally forward.
#ifdef debug
#pragma omp critical (cerr)
            cerr << "Starting from arbitrary node " << unvisited.begin()->first << " locally forward" << endl;
#endif

            s[unvisited.begin()->first] = unvisited.begin()->second;
            unvisited.erase(unvisited.begin()->first);
        }

        while (!s.empty()) {
            // Grab an oriented node
            auto n = s.begin()->second;
            s.erase(g->get_id(n));
            // Emit it
            sorted.push_back(n);
#ifdef debug
#pragma omp critical (cerr)
            cerr << "Using oriented node " << g->get_id(n) << " orientation " << g->get_is_reverse(n) << endl;
#endif

            // See if it has an edge from its start to the start of some node
            // where both were picked as places to break into cycles. A
            // reversing self loop on a cycle entry point is a special case of
            // this.
            g->follow_edges(n, true, [&](const handle_t& prev_node) {
                if(!unvisited.count(g->get_id(prev_node))) {
                    // Look at the edge
                    auto edge = g->edge_handle(prev_node, n);
                    if (masked_edges.count(edge)) {
                        // We removed this edge, so skip it.
                        return;
                    }
                    
#ifdef debug
#pragma omp critical (cerr)
                    cerr << "\tHas left-side edge to cycle entry point " << g->get_id(prev_node)
                         << " orientation " << g->get_is_reverse(prev_node) << endl;
#endif

                    // Mask the edge
                    masked_edges.insert(edge);
                    
#ifdef debug
#pragma omp critical (cerr)
                    cerr << "\t\tEdge: " << g->get_id(edge.first) << " " << g->get_is_reverse(edge.first)
                        << " -> " << g->get_id(edge.second) << " " << g->get_is_reverse(edge.second) << endl;
#endif
                }
            });

            // All other connections and self loops are handled by looking off the right side.

            // See what all comes next, minus deleted edges.
            g->follow_edges(n, false, [&](const handle_t& next_node) {

                // Look at the edge
                auto edge = g->edge_handle(n, next_node);
                if (masked_edges.count(edge)) {
                    // We removed this edge, so skip it.
                    return;
                }

#ifdef debug
#pragma omp critical (cerr)
                cerr << "\tHas edge to " << g->get_id(next_node) << " orientation " << g->get_is_reverse(next_node) << endl;
#endif

                // Mask the edge connecting these nodes in this order and
                // relative orientation, so we can't traverse it again

#ifdef debug
#pragma omp critical (cerr)
                cerr << "\t\tEdge: " << g->get_id(edge.first) << " " << g->get_is_reverse(edge.first)
                    << " -> " << g->get_id(edge.second) << " " << g->get_is_reverse(edge.second) << endl;
#endif

                // Mask the edge
                masked_edges.insert(edge);

                if(unvisited.count(g->get_id(next_node))) {
                    // We haven't already started here as an arbitrary cycle entry point

#ifdef debug
#pragma omp critical (cerr)
                    cerr << "\t\tAnd node hasn't been visited yet" << endl;
#endif

                    bool unmasked_incoming_edge = false;
                    g->follow_edges(next_node, true, [&](const handle_t& prev_node) {
                        // Get a handle for each incoming edge
                        auto prev_edge = g->edge_handle(prev_node, next_node);
                        
                        if (!masked_edges.count(prev_edge)) {
                            // We found such an edghe and can stop looking
                            unmasked_incoming_edge = true;
                            return false;
                        }
                        // Otherwise check all the edges on the left of this handle
                        return true;
                    });

                    if(!unmasked_incoming_edge) {

#ifdef debug
#pragma omp critical (cerr)
                        cerr << "\t\t\tIs last incoming edge" << endl;
#endif
                        // Keep this orientation and put it here
                        s[g->get_id(next_node)] = next_node;
                        // Remember that we've visited and oriented this node, so we
                        // don't need to use it as a seed.
                        unvisited.erase(g->get_id(next_node));

                    } else if(!seeds.count(g->get_id(next_node))) {
                        // We came to this node in this orientation; when we need a
                        // new node and orientation to start from (i.e. an entry
                        // point to the node's cycle), we might as well pick this
                        // one.
                        // Only take it if we don't already know of an orientation for this node.
                        seeds[g->get_id(next_node)] = next_node;

#ifdef debug
#pragma omp critical (cerr)
                        cerr << "\t\t\tSuggests seed " << g->get_id(next_node) << " orientation " << g->get_is_reverse(next_node) << endl;
#endif
                    }
                } else {
#ifdef debug
#pragma omp critical (cerr)
                    cerr << "\t\tAnd node was already visited (to break a cycle)" << endl;
#endif
                }
            });
        }
    }

    // Send away our sorted ordering.
    return sorted;

}

void sort(MutableHandleGraph* g) {
    if (g->node_size() <= 1) {
        // A graph with <2 nodes has only one sort.
        return;
    }
    
    // No need to modify the graph; topological_sort is guaranteed to be stable.
    
    // Topologically sort, which orders and orients all the nodes.
    vector<handle_t> sorted = topological_sort(g);
    
    size_t index = 0;
    g->for_each_handle([&](const handle_t& at_index) {
        // For each handle in the graph, along with its index
        
        // Swap the handle we observe at this index with the handle that we know belongs at this index.
        // The loop invariant is that all the handles before index are the correct sorted handles in the right order.
        // Note that this ignores orientation
        g->swap_handles(at_index, sorted.at(index));
        
        // Now we've written the sorted handles through one more space.
        index++;
    });
}

unordered_set<id_t> orient_nodes_forward(MutableHandleGraph* g) {
    // Topologically sort, which orders and orients all the nodes.
    vector<handle_t> sorted = topological_sort(g);
    
    // Track what we flip
    unordered_set<id_t> flipped;
    for (auto& handle : sorted) {
        if (g->get_is_reverse(handle)) {
            // This needs to be flipped
            flipped.insert(g->get_id(handle));
            // Flip it
            g->apply_orientation(handle);
        }
    }
    
    return flipped;
}
    
}
}
