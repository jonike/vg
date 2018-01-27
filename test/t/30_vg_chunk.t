#!/usr/bin/env bash

BASH_TAP_ROOT=../deps/bash-tap
. ../deps/bash-tap/bash-tap-bootstrap

PATH=../bin:$PATH # for vg

plan tests 15

# Construct a graph with alt paths so we can make a gPBWT and later a GBWT
vg construct -r small/x.fa -v small/x.vcf.gz -a >x.vg
vg index -x x.xg -v small/x.vcf.gz  x.vg
vg sim -x x.xg -l 100 -n 1000 -s 0 -e 0.01 -i 0.001 -a > x.gam
vg view -a x.gam > x.gam.json

# sanity check: does passing no options preserve input
is $(vg chunk -x x.xg -p x -c 10| vg stats - -N) 210 "vg chunk with no options preserves nodes"
is $(vg chunk -x x.xg -p x -c 10| vg stats - -E) 291 "vg chunk with no options preserves edges"

# check a small chunk
is $(vg chunk -x x.xg -p x:20-30 -c 0 | vg view - -j | jq -c '.path[0].mapping[].position' | jq 'select ((.node_id == 9))' | grep node | sed s/,// | sort | uniq | wc -l) 1 "chunk has path going through node 9"

# check no crash when using chunk_size, and filenames deterministic
rm -f _chunk_test*
vg chunk -x x.xg -p x -s 233 -o 50 -b _chunk_test -c 0 -t 2
vg chunk -x x.xg -p x -s 233 -o 50 -b _chunk_test -c 0 -t 1
is $(ls -l _chunk_test*.vg | wc -l) 6 "-s produces correct number of chunks"
rm -f _chunk_test*

#check that gam chunker runs through without crashing
vg index -a x.gam -d x.gam.unsrt.index
vg index -A -d x.gam.unsrt.index | vg index -N - -d x.gam.index
printf "x\t2\t200\nx\t500\t600\n" > _chunk_test_bed.bed
vg chunk -x x.xg -a x.gam.index -g -b _chunk_test -e _chunk_test_bed.bed -E _chunk_test_out.bed -c 0
is $(ls -l _chunk_test*.vg | wc -l) 2 "gam chunker produces correct number of graphs"
is $(ls -l _chunk_test*.gam | wc -l) 2 "gam chunker produces correct number of gams"
is $(grep x _chunk_test_out.bed | wc -l) 2 "gam chunker prodcues bed with correct number of chunks"

#check that id ranges work
is $(vg chunk -x x.xg -r 1:3 -c 0 | vg view - -j | jq .node | grep id |  wc -l) 3 "id chunker produces correct chunk size"
is $(vg chunk -x x.xg -r 1 -c 0 | vg view - -j | jq .node | grep id | wc -l) 1 "id chunker produces correct single chunk"

#check that traces work
is $(vg chunk -x x.xg -r 1:1 -c 2 -T | vg view - -j | jq .node | grep id | wc -l) 5 "id chunker traces correct chunk size"
is $(vg chunk -x x.xg -r 1:1 -c 2 -T | vg view - -j | jq -r '.path[] | select(.name == "thread_0") | .mapping | length') 3 "chunker can extract a partial haplotype"

# Check that traces work on a GBWT
# Reindex making the GBWT and not the gPBWT
vg index -x x.xg -G x.gbwt -v small/x.vcf.gz  x.vg
is "$(vg chunk -x x.xg -r 1:1 -c 2 -T | vg view - -j | jq -c '.path[] | select(.name != "x")' | wc -l)" 0 "chunker extracts no threads from an empty gPBWT"
is "$(vg chunk -x x.xg -G x.gbwt -r 1:1 -c 2 -T | vg view - -j | jq -c '.path[] | select(.name != "x")' | wc -l)" 2 "chunker extracts 2 local threads from a gBWT with 2 locally distinct threads in it"
is "$(vg chunk -x x.xg -G x.gbwt -r 1:1 -c 2 -T | vg view - -j | jq -r '.path[] | select(.name == "thread_0") | .mapping | length')" 3 "chunker can extract a partial haplotype from a GBWT"

#check that n-chunking works
# We know that it will drop _alt paths so we remake the graph without them for comparison.
vg construct -r small/x.fa -v small/x.vcf.gz >x.vg
mkdir x.chunk
vg chunk -x x.xg -n 5 -b x.chunk/
is $(cat x.chunk/*vg | vg view -V - 2>/dev/null | md5sum | cut -f 1 -d\ ) $(vg view x.vg | md5sum | cut -f 1 -d\ ) "n-chunking works and chunks over the full graph"

rm -rf x.gam.index x.gam.unsrt.index _chunk_test_bed.bed _chunk_test* x.chunk
rm -f x.vg x.xg xg.gbwt x.gam x.gam.json filter_chunk*.gam chunks.bed
