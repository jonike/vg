#ifndef VG_SET_H
#define VG_SET_H

#include <set>
#include <stdlib.h>
#include "gcsa.h"
#include "vg.hpp"
#include "index.hpp"
#include "hash_map.hpp"


namespace vg {

// for dealing with collections of VGs on disk
class VGset {
public:

    vector<string> filenames;

    VGset()
        : show_progress(false)
        { };

    VGset(vector<string>& files)
        : filenames(files)
        , show_progress(false)
        { };

    void transform(std::function<void(VG*)> lambda);
    void for_each(std::function<void(VG*)> lambda);

    // merges the id space of a set of graphs on-disk
    // necessary when storing many graphs in the same index
    int64_t merge_id_space(void);

    // stores the nodes in the VGs identified by the filenames into the index
    void store_in_index(Index& index);
    void store_paths_in_index(Index& index);

    // stores kmers of size kmer_size with stride over paths in graphs in the index
    void index_kmers(Index& index, int kmer_size, int edge_max, int stride = 1, bool allow_negatives = false);
    void for_each_kmer_parallel(function<void(string&, Node*, int, list<Node*>&, VG&)>& lambda,
                                int kmer_size, int edge_max, int stride, bool allow_dups, bool allow_negatives);
    
    // Write out kmer lines to GCSA2
    void write_gcsa_out(ostream& out, int kmer_size, int edge_max, int stride, bool allow_dups = true);
    
    // gets all the kmers in GCSA's internal format.
    void get_gcsa_kmers(int kmer_size, int edge_max, int stride, vector<gcsa::KMer>& kmers_out, bool allow_dups = true);

    bool show_progress;
    
private:
    
    // We create a struct that represents each kmer record we want to send to gcsa2
    struct KmerPosition {
        string kmer;
        string pos;
        set<char> prev_chars;
        set<char> next_chars;
        set<string> next_positions;
    };
    
    // We can loop over these in order to implement the other gcsa-related functions above.
    void for_each_gcsa_kmer_position_parallel(int kmer_size, int edge_max, int stride, function<void(KmerPosition&)> lambda,
                                              bool allow_dups = true);
    
};

}


#endif
