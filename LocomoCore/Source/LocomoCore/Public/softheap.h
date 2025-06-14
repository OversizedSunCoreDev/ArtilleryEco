/* File: softheap.h
 * MIT Licensed by Matt Millican, May 2016
 * ----------------
 * Header for a soft heap, an approximate min-priority queue that
 * allows amortized O(1) creation, merging, and extract-min
 * in exchange for reduced extract-min accuracy. The heap
 * takes a parameter epsilon and guarantees that in any sequence
 * of operations containing n inserts, there are never more than
 * epsilon * n "corrupted elements" in the heap: elements traveling 
 * with priorities higher than the priorities with which they 
 * were inserted. For a given value of epsilon, insertion into the 
 * soft heap is amortized O(log_2 (1/epsilon)).
 */

#pragma once
#include "CoreMinimal.h"


class FSoftheap
{
	/* An item in a soft heap tree node's list. */
	typedef struct LISTCELL
	{
		int elem;
		LISTCELL* next;
	} cell;

	
	
	/* A node in a tree in a soft heap. The node has access to its left and right children,
 * but does not need access to its parent. It contains a ckey (its priority), its rank,
 * the number of elements in its list, and its "size": a parameter defined such that
 * its list always contains Theta(size) elements so long as the node is not a leaf. 
 * Its list is stored as a doubly linked list. */
	typedef struct TREENODE
	{
		TREENODE* left;
		TREENODE* right;
		LISTCELL* first;
		LISTCELL* last;
		int ckey, rank, size, nelems;
	} node;
	

	/* Structure representing a binary tree in a soft heap's rootlist. The tree stores
 * its rank, which is the maximum possible height of its root (although the
 * root is not guaranteed to have that height at all times). The tree
 * is wired to its predecessor and successor in the rootlist, which have
 * rank less than and greater than this tree's rank, respectively. 
 * The tree also has a pointer to its own root.
 * 
 * Binary trees in a soft-heap are heap-ordered according to the "ckeys" of the nodes
 * in the trees. Each node stores a list of items under one ckey; the ckey is 
 * an upper bound on the original priorities of all items in the node's list.
 * The final element of a softheap tree is a pointer "sufmin" to the tree of minimum
 * root ckey in the segment of the rootlist beginning at this tree.
 */
	typedef struct TREE
	{
		struct TREE *prev, *next, *sufmin;
		struct TREENODE* root;
		int rank;
	} tree;


	
	/* Structure representing a soft heap. The soft heap object has access
	 * to the first tree in its root list, the rank of the highest-order tree
	 * in its root list, its error parameter epsilon, and the parameter
	 * r(epsilon) that defines the maximum node rank for which a node 
	 * is guaranteed to contain only uncorrupted elements. */
	typedef struct SOFTHEAP
	{
		struct TREE* first;
		int rank;
		double epsilon;
		int r;
	} softheap;





	/* Opaque type defining the soft heap data structure. */
	typedef struct SOFTHEAP softheap;

	/* Function: leaf
	 * --------------
	 * Return true if and only if this soft heap tree node
	 * has no children. 
	 */
	static inline bool leaf(node* x)
	{
		return (x->left == NULL && x->right == NULL);
	}

	/* Function: get_r
	 * ---------------
	 * Return the parameter r(epsilon) for this soft heap.
	 * r is the largest integer such that a node of that rank
	 * contains only uncorrupted elements.
	 */
	static inline int get_r(double epsilon)
	{
		return ceil(-log(epsilon) / log(2)) + 5;
	}

	/* Function: max 
	 * -------------
	 * Return the larger of two doubles. 
	 */
	static double max(double x, double y)
	{
		return (x >= y ? x : y);
	}

	/* Function: min
	 * -------------
	 * Return the smaller of two doubles.
	 */
	static double min(double x, double y)
	{
		return (x <= y ? x : y);
	}

	/* Function: swapLR 
	 * ----------------
	 * Swap the left and right children of this node.
	 */
	static inline void swapLR(node* x)
	{
		node* tmp = x->left;
		x->left = x->right;
		x->right = tmp;
	}

	/* Function: get_next_size
	 * -----------------------
	 * Get the size of a soft heap node with given rank.
	 * Given a parameter r(epsilon) for a soft heap, the size
	 * of a node of rank k is 1 (if k <= r) or ceil(3/2 * size(k-1))
	 * otherwise. 
	 */
	static inline int get_next_size(int rank, int prevrank_size, int r)
	{
		if (rank <= r) return 1;
		return (3 * prevrank_size + 1) / 2;
	}

	/**************************************** HEAP & ITEM CREATION *************************************/

	/* Function: addcell
	 * -----------------
	 * Creates a list cell containing the parameter element
	 * and concatenates it to the end of the linked list pointed
	 * to by listend.
	 */
	static cell* addcell(int elem, cell* listend)
	{
		cell* c = new cell();
		c->elem = elem;
		if (listend != NULL) listend->next = c;
		c->next = NULL;
		return c;
	}

	/* Function: makenode
	 * ------------------
	 * Constructs a rank-0 soft heap binary tree node containing just the parameter
	 * element. Its ckey matches the element, since that element is the only
	 * object in its list.
	 */
	static node* makenode(int elem)
	{
		node* x = new node();
		x->first = x->last = addcell(elem, NULL);
		x->ckey = elem;
		x->rank = 0;
		x->size = x->nelems = 1;
		x->left = x->right = NULL;
		return x;
	}

	/* Function: maketree
	 * ------------------
	 * Constructs a soft heap binary tree consisting of exactly one node
	 * housing the parameter element.
	 */
	static tree* maketree(int elem)
	{
		tree* T = new tree();
		T->root = makenode(elem);
		T->prev = T->next = NULL;
		T->rank = 0;
		T->sufmin = T;
		return T;
	}

	/* Function: makeheap
	 * ------------------
	 * Construct a soft heap with error parameter epsilon containing element elem.
	 * This is done by constructing a tree of rank 0 containing a single rank-0
	 * node. The node has one item in its item list, which is the item inserted.
	 */
	softheap* makeheap(int elem, double epsilon)
	{
		softheap* s = makeheap_empty(epsilon);
		s->first = maketree(elem);
		s->rank = 0;
		return s;
	}

	/* Function: makeheap_empty
	 * ------------------------
	 * Constructs an empty soft heap with the provided error parameter.
	 */
	softheap* makeheap_empty(double epsilon)
	{
		softheap* s = new softheap();
		s->first = NULL;
		s->rank = -1; // Ensures that any insertion will just return the SH containing the inserted elem
		s->epsilon = epsilon;
		s->r = get_r(epsilon);
		return s;
	}

	/* Function: destroy_node
	 * ----------------------
	 * Deallocates all the cells in this node's item list,
	 * recursively destroys its left and right children, 
	 * then deallocates its memory. For use in destroy_heap.
	 */
	static void destroy_node(node* treenode);

	/* Function: destroy_heap
	 * ----------------------
	 * Destroys this soft heap and deallocates all its associated memory
	 * by iterating over its list of trees, destroying them all, and then
	 * destroying the heap struct.
	 */
	void destroy_heap(softheap* P);
	
	/* Function: moveList
	 * ------------------
	 * Remove the item list of src and append it to the end
	 * of the item list of dst.
	 */
	static void moveList(node* src, node* dst);

	/* Function: sift
	 * --------------
	 * The primary reorganizational strategy of the soft heap, called whenever
	 * a non-leaf soft heap tree node has fewer items in its list than it should 
	 * according to its rank. The parameter node x steals the item list and ckey
	 * of whichever child has lower ckey, which pushes the length of its list above
	 * its size paremeter while maintaining the heap property with respect to ckeys.
	 * Then, to repair the child (which is now deficient as x once was), we recursively
	 * call sift on the child (unless it was a leaf, in which case it cannot be repaired).
	 * Once x's child has been repaired or destroyed, x itself may or may not still be
	 * deficient; if it is still deficient and has not become a leaf, we repeat the process
	 * of stealing from children and recursively repairing children until x is repaired or a leaf.
	 */
	static void sift(node* x);

	/* Function: combine
	 * -----------------
	 * Another important restructuring operation, used whenever we merge two trees of equal rank.
	 * Creates a new node z with children x and y and rank 1 + rank(x), sets its size parameter,
	 * and then fills its list by sifting through its children. 
	 */
	static node* combine(node* x, node* y, int r);

	/* Function: insert_tree
	 * ---------------------
	 * Inserts a tree from the rootlist of some external heap into the rootlist of
	 * into_heap, immediately before the tree pointed to by successor.
	 * Wires it into the pointer structure of the heap as necessary, including
	 * making it the first tree of into_heap if the tree pointed to by successor
	 * has no predecessors.
	 */
	static void insert_tree(softheap* into_heap, tree* inserted, tree* successor);

	/* Function: remove_tree
	 * ---------------------
	 * Removes the soft heap tree pointed to by removed from the parameter heap.
	 * This entails wiring its predecessor and successor in the heap's rootlist
	 * to each other (if they exist) and setting the heap's first tree to be
	 * the removed tree's successor if the removed tree was the first in the rootlist.
	 */
	static void remove_tree(softheap* outof_heap, tree* removed);

	/* Function: update_suffix_min
	 * ---------------------------
	 * Updates the sufmin pointers of T and all trees preceding T in T's rootlist.
	 * This should be done whenever heap restructuring affects a segment of the 
	 * rootlist ending at T, i.e. if an element is extracted from T, if T is the 
	 * final tree created by a soft heap meld, or if T's successor is removed.
	 * Whenever any of these occur, the segment of the heap ending at T may have
	 * a new root of minimum ckey, meaning every sufmin pointer until T must be edited.
	 * Given the recursive definition of a sufmin pointer this is easy to revise by
	 * moving backwards from T.
	 */
	static void update_suffix_min(tree* T);

	/* Function: merge_into
	 * --------------------
	 * The first step of soft heap melding. Given a soft heap P whose rank is no more
	 * than that of heap Q, walk through the root lists of both heaps, placing each tree
	 * from P immediately before the first tree of Q with equal or greater rank.
	 */
	static void merge_into(softheap* P, softheap* Q);

	/* Function: repeated_combine
	 * --------------------------
	 * The second step of soft heap melding. Now that all trees of equal rank from the
	 * original two heaps are adjacent in the larger heap, this process simulates
	 * binary addition using a binomial heap-like strategy in which trees of equal rank
	 * are merged and the results are "carried" until a vacancy is found for the rank
	 * of the resulting combined tree. We only operate on the heap until we find
	 * a tree of rank greater than the smaller (original) heap's rank that doesn't need
	 * to be merged with its successor, at which point no successor trees can possibly
	 * have partners and merging is no longer necessary.
	 */
	static void repeated_combine(softheap* Q, int smaller_rank, int r);

	/* Function: extract_elem
	 * ----------------------
	 * Remove the first element from the item list of node x and return it.
	 * To reflect this change, decrement x's nelems counter, change the
	 * element it points to as the first item, rewire the prev pointer
	 * of the new first element to NULL (if it exists), and reset the
	 * last pointer of x if the new list has one or no items.
	 */
	static int extract_elem(node* x);

	/*************************************** CLIENT-SIDE OPERATIONS ************************************/

	/* Function: empty
	 * ---------------
	 * Returns true if and only if P contains no trees,
	 * i.e. it contains no elements.
	 */
	bool empty(softheap* P);

	/* Function: insert
	 * ----------------
	 * Put a new element into soft heap P. If P is nonempty, this can be accomplished 
	 * by creating a new soft heap for the parameter and melding it into P. However,
	 * if P is empty, this strategy will destroy P and leave the client with a freed
	 * pointer, so instead we directly insert a new tree containing elem into P's rootlist
	 * and set its rank to 0.
	 */
	void insert(softheap* P, int elem);

	/* Function: meld
	 * --------------
	 * Combine all elements of soft heaps P and Q into a new conglomerate heap,
	 * destructively modifying P and Q. Return the result. This is implemented
	 * by executing a merge_into to push all elements from the lower-rank heap
	 * into the higher-rank heap, then calling repeated-combine to combine
	 * all trees of duplicate rank.
	 */
	softheap* meld(softheap* P, softheap* Q);

	/* Function: extract_min
	 * ----------------------
	 * Extract and return an element from the node of minimum ckey 
	 * in the soft heap. 
	 */
	int extract_min(softheap* P)
	{
		int filler; // I'm just here to prevent code duplication
		return extract_min_with_ckey(P, &filler);
	}

	/* Function: extract_min_with_ckey
	 * -------------------------------
	 * Extract and return an element from the node of minimum ckey
	 * in the soft heap, and store that ckey in the space pointed to
	 * by ckey_into. The node of minimum ckey is the root of some
	 * tree in the heap, by the heap property invariant. This tree
	 * is pointed to by the sufmin pointer of the first tree in the rootlist.
	 * After removing that element from the root, we check whether it is now
	 * size-deficient. If so, we sift it (if it has children), ignore it
	 * (if it has no children but is not empty), or destroy the tree 
	 * it roots (if it has no children and is empty). Once this is done, we
	 * update the sufmin pointers of T and all its predecessors
	 * (or just T's predecessors if T was removed).
	 */
	int extract_min_with_ckey(softheap* P, int* ckey_into);
};
