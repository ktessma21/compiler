## Avaiable Munches 
1. normal plus/minus (cost 2) => clear case both number or var.
2. Shifting (cost 1) => works only if the tree branch has multiples or power of two. 
3. multiplication by *= num (cost 2) => works 
4. mem var const (cost 1) => great.  the const could be given as a binop if the second bottom does match then of course we can match this too. => i.e if it has a var and a number 
    - if it doesn't match then maybe you can force the bottom to match by recursion and then match the top. You can do all the combination of munching. no heaurestic for now. 


# after munching well replace the munches with instruction and continue removing or unveiling the tile. 




# we can't implement the dynamic programming since it is NP-complete and only Polynomial for trees. but our most instrucitons form a DAG. so we will implement a greedy optimum instead. 

## focus on clear tree building. 
## each context 
