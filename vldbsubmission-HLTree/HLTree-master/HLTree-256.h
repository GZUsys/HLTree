#include <cassert>
#include <climits>
#include <fstream>
#include <future>
#include <iostream>
#include <atomic>
#include <math.h>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <libpmemobj.h>
#include <immintrin.h>
#include "nodepref.h"
#define USE_PMDK

// the size of a tree node
#define NONLEAF_LINE_NUM        4    // 256B
#define LEAF_LINE_NUM           4    // 256B

// the number of leaf nodes to prefetch ahead in jump pointer array 
// prefetching
#define PREFETCH_NUM_AHEAD	3


// node size
#define NONLEAF_SIZE    (CACHE_LINE_SIZE * NONLEAF_LINE_NUM)
#define LEAF_SIZE       (CACHE_LINE_SIZE * LEAF_LINE_NUM)

// key size and pointer size: 8B
typedef long long key_type;
#define KEY_SIZE             8   /* size of a key in tree node */
#define POINTER_SIZE         8   /* size of a pointer/value in node */
#define ITEM_SIZE            8   /* key size or pointer size */

#define MAX_KEY		((key_type)(0x7fffffffffffffffULL))
#define MIN_KEY		((key_type)(0x8000000000000000ULL))
#define bitScan(x)  __builtin_ffs(x) //return the 1 location in the end of x,from 0 to n
#define countBit(x) __builtin_popcount(x) //return the number of bit 1

typedef long long key_type;
typedef unsigned short uint16;
typedef unsigned long long uint64;
typedef unsigned int version_t;
#define MAX_ENTRIES 13 
#define LEAF_KEY_NUM    (13) 

#define  NON_LEAF_ENTRYS 15
#define NON_LEAF_KEY_NUM    (NONLEAF_SIZE/(KEY_SIZE+POINTER_SIZE)-1)

extern long splitnum;
extern long clwbnum;
extern long sfencenum;

static inline unsigned char hashcode1B(key_type x) {
    x ^= x >> 32;
    x ^= x >> 16;
    x ^= x >> 8;
    return (unsigned char)(x & 0x0ffULL);
}


static inline
void sfence(void)
{
     asm volatile("mfence":::"memory"); //sfencenum++;
    //asm volatile("sfence"); //sfencenum++;
}


static inline
void clwb(void* addr)
{
    //asm volatile("clwb %0": : "m"(*((char*)addr))); //clwbnum++;
    asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(addr)));//clwbnum++;
}

/**
 * flush [start, end]
 *
 * there are at most two lines.
 */
static inline
void clwb2(void* start, void* end)
{
    if (getline(start) != getline(end)) {
        clwb(end);
        clwb(start);
    }
    else {
        clwb(start);
    }
}

/**
 * flush [start, end]
 *
 * there can be 1 to many lines
 */
static inline
void clwbmore(void* start, void* end)
{
    unsigned long long start_line = getline(start);
    unsigned long long end_line = getline(end);
    do {
        clwb((char*)start_line);
        start_line += CACHE_LINE_SIZE;
    } while (start_line <= end_line);
}

/**
 * Pointer8B defines a class that can be assigned to either bnode or bleaf.
 */
class Pointer8B {
public:
    unsigned long long  value;  /* 8B to contain a pointer */

public:
    Pointer8B() {}

    Pointer8B(const void* ptr)
    {
        value = (unsigned long long)ptr;
    }

    Pointer8B(const Pointer8B& p)
    {
        value = p.value;
    }

    Pointer8B& operator= (const void* ptr)
    {
        value = (unsigned long long)ptr;
        return *this;
    }
    Pointer8B& operator= (const Pointer8B& p)
    {
        value = p.value;
        return *this;
    }

    bool operator== (const void* ptr) {
        bool result = (value == (unsigned long long)ptr);
        return result;
    }
    bool operator== (const Pointer8B& p) {
        bool result = (value == p.value);
        return result;
    }


    operator void* () { return (void*)value; }
    operator char* () { return (char*)value; }
    operator struct Bnode* () { return (struct Bnode*)value; }
    operator struct Lnode* () { return (struct Lnode*)value; }
    operator unsigned long long() { return value; }

    bool isNull(void) { return (value == 0); }

    void print(void) { printf("%llx\n", value); }

}; // Pointer8B


class VersionedLock {
private:
    std::atomic<version_t> lock_version;
    version_t rw_version;

public:
    void initlock() { rw_version = 0; lock_version.store(2, std::memory_order_release); }
    version_t write_lock() {
        version_t ver = lock_version.load(std::memory_order_acquire);
        if (ver < 2) {
            version_t new_ver = 3;
            if (lock_version.compare_exchange_weak(ver, new_ver)) {

                rw_version = 0;
                return new_ver;
            }
            return 0;
        }
        else
        {
            if ((ver & 1) == 0 && lock_version.compare_exchange_weak(ver, ver + 1))
            {
                return ver;
            }
        }
        return 0;
    }

    void write_unlock() {
        lock_version.fetch_add(1, std::memory_order_release);
        return;
    }

    void set_readversion() {
        rw_version++;
        return;
    }

    version_t get_readversion() {
        return rw_version;
    }
};


//node entry
typedef struct entry {
    key_type key;
    Pointer8B pv;
}entry;

//leaf-node metdata
typedef struct Lhead {
    uint16_t bitmap : 13;
    uint16_t hangin : 1;
    uint16_t deleted : 1;
    uint16_t unused : 1;
    unsigned char  fingerPrint[MAX_ENTRIES];
    unsigned char area;
}Lhead;

//leaf node
class Lnode {
public:
    Lhead head;  
    VersionedLock Llock;//8B
    Lnode* next;  //8B
    key_type Min_Key;
    key_type Max_Key;
    entry ent[LEAF_KEY_NUM];
public:

    bool writelock() { return Llock.write_lock(); }
    void writeunlock() { return Llock.write_unlock(); }
    void setreadversion() { return Llock.set_readversion(); }
    version_t getreadversion() { return Llock.get_readversion(); }
    void init() { 
	    next = NULL;
	    Min_Key=0;
	    Max_Key=0; 
	    head.bitmap = 0; 
	    head.hangin = 0; 
	    head.unused=0;
        head.area=0;
	    head.deleted = 0; 
	    Llock.initlock(); 
    }
    key_type k(int idx) { return ent[idx].key; }
    Pointer8B ch(int idx) { return ent[idx].pv; }
    key_type getminkey(void)
    {
        return Min_Key;
    }
    key_type getmaxkey(void)
    {
        return Max_Key;
    }

    int num() { return countBit(head.bitmap); }

    key_type getdistance(void) {
    
    int s_point= LEAF_KEY_NUM / 2;
    key_type distance;
    key_type result;
    int knum=num();
    if(num() <= 2){return MAX_KEY;}
    key_type min = getminkey();
    key_type max = getmaxkey();
    key_type total=max - min;
    if(num() < s_point){
    result = total/(knum-1);
    distance = result * (s_point - knum) + max;
    if(distance<0)
    {
        distance=MAX_KEY;
    }
    return distance;
    }
    else{
    result = total/LEAF_KEY_NUM;
    distance= max + result;
    if(distance<0)
    {
	distance=MAX_KEY;
    }
    return distance;
    }
    }

    bool isFull(void) { return (head.bitmap == 0x1fff);}
    bool isNull(void) { return (head.bitmap == 0x0); }
    //int num() { return countBit(head.bitmap); }
    bool isjump() { return (num() >= (LEAF_KEY_NUM / 2)); }
    friend class VersionedLock;
    friend class Pointer8B;
};

// non-leaf node
class Bnode {
public:
    std::atomic<int> block;
    int num;
    Pointer8B left_sibling;
    entry ent[NON_LEAF_ENTRYS];
public:
    key_type& k(int idx) { return ent[idx].key; }
    Pointer8B& ch(int idx) { return ent[idx].pv; }

    char* chEndAddr(int idx) {
        return (char*)&(ent[idx].pv) + sizeof(Pointer8B) - 1;
    }

    void initlock(void)
    { 
	    block.store(2, std::memory_order_release);
        num = 0;
    }
    int getlock(void)
    {
        int ver = block.load(std::memory_order_acquire);
        if ((ver & 1) == 0)
        {
            return 0;
        }
        return 1;
    }

    int getlockver(void)
    {
        return block.load(std::memory_order_acquire);
    }

    int bwlock(void)
    {
        int ver = block.load(std::memory_order_acquire);
        if (ver < 2) {
            int new_ver = 3;
            if (block.compare_exchange_weak(ver, new_ver)) {
                return 1;
            }
            return 0;
        }
        else
        {
            if ((ver & 1) == 0 && block.compare_exchange_weak(ver, ver + 1))
            {
                return 1;
            }
        }
        return 0;
    }
    void bwunlock(void) {
        block.fetch_add(1, std::memory_order_release);
        return;
    }
    friend class Pointer8B;
};

// TreeMeta
class treeMeta {
public:
    Pointer8B  tree_root;  // the tree root
    Lnode* first_leaf; //the first leaf node on PM 
    int root_level; //0 behalf of the leaf node,1 or ...n behalf of the non-leaf node
public:
    treeMeta(void* nvm_address, bool recover = false)
    {
        tree_root = NULL;
        root_level = 0;
        first_leaf = (Lnode*)nvm_address;
       // if (!recover) setFirstLeaf(NULL);
    }
    void setFirstLeaf(Lnode* leaf)
    {
        first_leaf = leaf;	
        clwb(first_leaf);
        //sfence();
    }
    friend class Lnode;
    friend class Bnode;
    friend class btree;
};

#ifdef USE_PMDK
POBJ_LAYOUT_BEGIN(btree);
POBJ_LAYOUT_TOID(btree, Lnode);
POBJ_LAYOUT_END(btree);
PMEMobjpool* pop;
#endif

void* alloc(size_t size) {
    //PMEMoid bp;
    //POBJ_ZALLOC(pop, &bp, Lnode, size);
    //pmemobj_alloc(pop, &bp, sizeof(Lnode), 0, NULL, NULL);
    TOID(Lnode) bp;
    POBJ_ZALLOC(pop, &bp, Lnode, size);
    return pmemobj_direct(bp.oid);
}

void* Ialloc(size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
}

int file_exists(const char* filename) {
    struct stat buffer;
    return stat(filename, &buffer);
}

void openPmemobjPool() {
    printf("use pmdk!\n");
    char pathname[100] = "/mnt/ext4/utree/pool-main";
    int sds_write_value = 0;
    pmemobj_ctl_set(NULL, "sds.at_create", &sds_write_value);
    if (file_exists(pathname) != 0) {
        printf("create new one.\n");
        if ((pop = pmemobj_create(pathname, POBJ_LAYOUT_NAME(btree),
            (uint64_t)200 * 1024 * 1024 * 1024, 0666)) ==
            NULL) {
            perror("failed to create pool.\n");
            return;
        }
    }
    else {
        printf("open existing one.\n");
        if ((pop = pmemobj_open(pathname, POBJ_LAYOUT_NAME(btree))) == NULL) {
            perror("failed to open pool.\n");
            return;
        }
    }

}



class btree {
  public:  // root and level
      treeMeta* tree_meta;

  public:
      btree(void* nvm_address, bool recover = false)
      {	     
          tree_meta = new treeMeta(nvm_address, recover);
          if (!tree_meta) { perror("new"); exit(1); }         
      }

      ~btree()
      {
          delete tree_meta;
        }

      void* get_recptr(void* p, int pos)
      {
          return ((Lnode*)p)->ch(pos);
      }

      // given a search key, perform the search operation
      void* lookup(key_type key, int* pos);

      //sort and return  the keys of node
      void sortLnode(Lnode* p, int start, int end, int pos[]);

      //update bnode
      void Update_Bnode(key_type key, Pointer8B pt);
      // insert (key, ptr)
      bool insert(key_type key, void* ptr);

      // delete key
      bool del(key_type key);

      void scan(key_type Bkey, key_type Ekey, long len);

      void checkFirstLeaf(void);

      void delBnode(key_type key);

      void print();

      void check(key_type start, key_type end, long len) {
          scan(start, end, len);
      }

      int level() { return tree_meta->root_level; }

      void set_first_leaf(key_type key, void* ptr);

     
      friend class treeMeta;
      friend class Lnode;
      friend class Bnode;  
      friend class Pointer8B;
};


/* ----------------------------------------------------------------- *
 look up
 * ----------------------------------------------------------------- */

 /* leaf is level 0, root is level depth-1 */

void* btree::lookup(key_type key, int* pos)
{
    Bnode* p;
    Lnode* lp;
    Bnode* parent;
    int i, t, b, m;
    key_type r;
    int oldver;
    unsigned char key_hash = hashcode1B(key);
    int ret_pos;
restart:
    // 2. search nonleaf nodes
    p = tree_meta->tree_root;
    for (i = tree_meta->root_level; i > 0; i--) {
        // if the lock bit is set, retry
        parent = p;
        oldver = p->getlockver();
        if (p->getlock()) { goto restart; }
        b = 0; t = p->num-1;
        while (b + 7 <= t) {
            m = (b + t) >> 1;
            r = key - p->k(m);
            if (r > 0) b = m + 1;
            else if (r < 0) t = m - 1;
            else { p = p->ch(m); goto inner_done; }
        }
        // sequential search
        for (; b <= t; b++) {
            if (key < p->k(b))break;
        }
        if (b == 0)
        {
            p = p->left_sibling;
        }
        else
        {
            p = p->ch(b - 1);
        }
    inner_done:
        if (oldver != parent->getlockver())
        {
            goto restart;
        }
    }
    int pw = 0;
    // 3. search leaf node
    lp = (Lnode*)p;
    //look up the next leaf node
    while (lp->next != NULL && key > lp->getmaxkey())
    {
        lp = lp->next;
        pw++;
    }
    //get the current lock version
    //printf("min max key is %lld, %lld, %lld, %ld\n",lp->Min_Key,lp->Max_Key, lp->num(),lp->head.hangin);
    version_t oldversion = lp->getreadversion();
    // search every matching candidate
    uint16_t fbitmap = lp->head.bitmap;
    ret_pos = -1;
    while (fbitmap) {
        int jj = bitScan(fbitmap) - 1;  // next candidate
	if(lp->head.fingerPrint[jj] == key_hash)
        if (lp->k(jj) == key) { // found
            ret_pos = jj;
            break;
        }
	//printf("key is %lld,%lld\n",jj,lp->k(jj));
        fbitmap &= ~(0x1 << jj);  // remove this bit
    } // end while
     
    if (ret_pos < 0) { return NULL; }
    if (lp->getreadversion() != oldversion) { goto restart; }
    *pos = ret_pos;
    /*
    if (pw > 2 && lp->head.hangin == 1) {
        lp->head.hangin = 0;
        Update_Bnode(lp->getminkey(), lp);
    }
    */
    return (void*)lp->k(ret_pos);
}

/* ------------------------------------- *
   quick sort the keys in leaf node
 * ------------------------------------- */


 // pos[] will contain sorted positions
void btree::sortLnode(Lnode* p, int start, int end, int pos[])
{
    if (start >= end) return;

    int pos_start = pos[start];
    key_type key = p->k(pos_start);  // pivot
    int l, r;

    l = start;  r = end;
    while (l < r) {
        while ((l < r) && (p->k(pos[r]) > key)) r--;
        if (l < r) {
            pos[l] = pos[r];
            l++;
        }
        while ((l < r) && (p->k(pos[l]) <= key)) l++;
        if (l < r) {
            pos[r] = pos[l];
            r--;
        }
    }
    pos[l] = pos_start;
    sortLnode(p, start, l - 1, pos);
    sortLnode(p, l + 1, end, pos);
}

/* set first leaf node */
void btree::set_first_leaf(key_type key, void* ptr)
{
    //create first leaf node 
    Lnode* newp = (Lnode*)alloc(LEAF_SIZE);
    newp->init();
    newp->ent[0].key = key;
    newp->ent[0].pv = ptr;
    newp->head.fingerPrint[0] = hashcode1B(key);
    newp->head.bitmap |= 1;
    newp->head.hangin = 0;
    newp->Min_Key=key;
    newp->Max_Key=key;
    tree_meta->setFirstLeaf(newp);
    tree_meta->tree_root = newp;
    tree_meta->root_level = 0;
    sfence();
    return;
}

/* ---------------------------------------------------------- *

 insertion: insert (key, ptr) pair into unsorted_leaf

 * ---------------------------------------------------------- */

bool btree::insert(key_type key, void* ptr)
{
    unsigned char key_hash = hashcode1B(key);
    Bnode* p;
    Bnode* parent;
    Lnode* lp;
    int i, t, m, b;
    int pw;
    int oldver;
    key_type r;
restart:
    // search nonleaf nodes
    pw = 0;
    p = tree_meta->tree_root;
    for (i = tree_meta->root_level; i > 0; i--) {
        // if the lock bit is set, retry
        if (p->getlock()) { goto restart; }
        parent = p;
        oldver = p->getlockver();
        b = 0; t = p->num-1;
        while (b + 7 <= t) {
            m = (b + t) >> 1;
            r = key - p->k(m);
            if (r > 0) b = m + 1;
            else if (r < 0) t = m - 1;
            else { p = p->ch(m);  goto inner_done; }
        }
        // sequential search
        for (; b <= t; b++)
        {
            if (key < p->k(b)) break;
        }
        if (b == 0)
        {
            p = p->left_sibling;
        }
        else
        {
            p = p->ch(b - 1);
        }       
    inner_done:

        if (oldver != parent->getlockver())
        {
            goto restart;
        }

    }

    // search leaf node
    lp = (Lnode*)p;
    //find the node of insert key
  //printf("min max key is %lld, %lld, %lld, %ld\n",lp->Min_Key,lp->Max_Key, lp->num(),lp->head.hangin);
    while (lp->next != NULL && key > lp->getmaxkey())
    {
        Lnode* tempt;
        tempt = lp->next;
        while (tempt->head.deleted && tempt->next != NULL)
        {
		//printf("error node is occur\n");
            tempt = tempt->next;
        }
        if (key >= tempt->getminkey())
        {
	    if(tempt->head.hangin==0)
	    printf("error node is occur\n");
            pw++;
            lp = tempt;
        }
        else
        {
            break;
        }
    }

    // if the lock is set, retry
    if (!lp->writelock()) { goto restart; }
    if(key <= lp->Max_Key)
    {
    uint16_t fbitmap = lp->head.bitmap;
    while (fbitmap) {
        int jj = bitScan(fbitmap) - 1;  // next candidate
	if(lp->head.fingerPrint[jj] == key_hash)
        if (lp->k(jj) == key) { // found: do nothing, return
            lp->writeunlock();
            return true;
        }
        fbitmap &= ~(0x1 << jj);  // remove this bit
    } // end while
    }
    // end of Part 1
    /* Part 2. leaf node */
    /* 1. leaf is not full */
    if (!lp->isFull()) {
	   
            //prdecit split,insert into new node
            if ((key > lp->getdistance()) && (tree_meta->root_level > 0))
            {
                if (lp->next !=NULL && lp->next->head.hangin)
                {
                    //node is hang in leaf-node layer
                        lp->writeunlock();
                        lp = lp->next;
                        if (!lp->writelock()) {goto restart;}
                        // get first empty slot
			uint16_t bitmap = lp->head.bitmap;
                        int slot = bitScan(~bitmap) - 1;
                        lp->ent[slot].key = key;
                        lp->ent[slot].pv = ptr;
                        lp->head.fingerPrint[slot] = key_hash;
                        bitmap |= (1 << slot);
                        lp->head.bitmap = bitmap;
			if(key > lp->Max_Key)
                        {
                           lp->Max_Key = key;
                        }
                        else
                        {
                           if(key < lp->Min_Key)
                           {
                                lp->Min_Key=key;
                           }
                        }

			clwb2(lp,&(lp->ent[slot]));
                        sfence();

                        if(lp->isjump())
                        {
                        lp->head.hangin=0;
                        Update_Bnode(lp->getminkey(), lp);
                        }
                        //release write lock
                        lp->writeunlock();
                        return true;
                }

                // next node not is hang node, new node
                Lnode* newp = (Lnode*)alloc(LEAF_SIZE);
		newp->init();
		newp->writelock();
                newp->ent[0].key = key;
                newp->ent[0].pv = ptr;
                newp->head.fingerPrint[0] = key_hash;
		newp->Min_Key=key;
                newp->Max_Key=key;
		newp->head.hangin = 1;
                newp->head.deleted = 0;
		newp->head.bitmap|= 1;
                newp->next = lp->next;
                lp->next = newp;
                //cflush and sfence newp
                clwb(newp);
                sfence();
                newp->writeunlock();
                //cflush and sfence lp
                clwb(lp);
                sfence();
                lp->writeunlock();
                return true;
        }
	
    //insert to the current node
    // get first empty slot
	uint16_t bitmap = lp->head.bitmap;
        int slot = bitScan(~bitmap) - 1;
        lp->ent[slot].key = key;
        lp->ent[slot].pv = ptr;
        lp->head.fingerPrint[slot] = key_hash;
        if (key > lp->Max_Key)
        {
            lp->Max_Key = key;
        }
        else 
        {
            if (key < lp->Min_Key)
            {
                lp->Min_Key = key;
            }
        }
	lp->head.bitmap |= (1 << slot);
        // flush the line containing slot
        clwb2(lp, &(lp->ent[slot]));
        sfence();
	if(tree_meta->root_level>0){
         if(lp->isjump() && lp->head.hangin)
         {
        	lp->head.hangin = 0;
        	Update_Bnode(lp->getminkey(), lp);
         }
	 }
	 
          //release write lock
        lp->writeunlock();
        return true;
    }
    // end of not full 
    /* 2. leaf is full, split */
    //  get sorted positions
    else
    {
        int split;
        int location;
        int insert_slot = 0;
        key_type split_key;
        Lnode* temptnode;
        int sorted_pos[LEAF_KEY_NUM];
        for (i = 0; i < LEAF_KEY_NUM; i++) sorted_pos[i] = i;
        sortLnode(lp, 0, LEAF_KEY_NUM - 1, sorted_pos);
        /*the sibling node is deleted node*/
	
        if ((lp->next != NULL) && (lp->next->head.deleted)) {
            if (!lp->next->writelock()) {
                lp->writeunlock();
                goto restart;
            }
            temptnode = lp->next;            
        }

        else {
            Lnode* newp = (Lnode*)alloc(LEAF_SIZE);
            newp->init();
            newp->writelock();
            temptnode = newp;          
        }
        uint16_t freed_slots = lp->head.bitmap;
        uint16_t newbitmap = 0;

        split = (LEAF_KEY_NUM / 2);
        split_key = lp->k(sorted_pos[split]);        

        location = split;
        //  key > split_key: insert key into new node
        if (key > split_key) {
            temptnode->ent[split].key = key;
            temptnode->ent[split].pv = ptr;
            temptnode->head.fingerPrint[split] = key_hash;
            newbitmap |= (1 << split);
            split = split + 1;
        }
        //move key from old node to new node
        for (int i = split; i < LEAF_KEY_NUM; i++)
        {
            temptnode->ent[i] = lp->ent[sorted_pos[i]];
            temptnode->head.fingerPrint[i] = lp->head.fingerPrint[sorted_pos[i]];
            newbitmap |= (1 << i);
            freed_slots &= ~(1 << sorted_pos[i]);
        }

        clwb2(&(temptnode->ent[location]), &(temptnode->ent[LEAF_KEY_NUM - 1]));
        sfence();
        if (!temptnode->head.deleted)
        {
            temptnode->next = lp->next;
            lp->next = temptnode;
        }
        temptnode->head.hangin = 1;
        temptnode->head.deleted = 0;
        temptnode->head.bitmap = newbitmap;
	temptnode->Min_Key=temptnode->k(split);
        temptnode->Max_Key=temptnode->k(LEAF_KEY_NUM-1);
        if(split != location)
        {
           if(key < temptnode->k(split))
           {
              temptnode->Min_Key=key;
           }
           else
           {
             if(key > temptnode->k(LEAF_KEY_NUM-1))
             {
                temptnode->Max_Key=key;
             }
           }
        }
        clwb(temptnode);
        sfence();
 
        lp->head.bitmap = freed_slots;
	lp->Max_Key = lp->k(sorted_pos[split-1]);
        //set read version
        lp->setreadversion();
        clwb(lp);
        sfence();

        //  key < split_key: insert key into old node 
        if (key < split_key)
        {
            uint16_t lbitmap = lp->head.bitmap;
            int slot = bitScan(~lbitmap) - 1;
            insert_slot = slot;
            lp->ent[slot].key = key;
            lp->ent[slot].pv = ptr;
            lp->head.fingerPrint[slot] = key_hash;
            lbitmap |= (1 << slot);

            if (key > lp->Max_Key)
            {
                lp->Max_Key = key;
            }
            else
            {
                if (key < lp->Min_Key)
                {
                    lp->Min_Key = key;
                }
            }

	    lp->head.bitmap = lbitmap;
        }

        // clwb lp and sfence
        clwb2(lp, &(lp->ent[insert_slot]));
        sfence();
	
        if(tree_meta->root_level==0){
        temptnode->head.hangin = 0;
        Update_Bnode(temptnode->getminkey(), temptnode);
         }
	 
        temptnode->writeunlock();
        lp->writeunlock();
        return true;
    }
    return false;
}

void btree::Update_Bnode(key_type key, Pointer8B pt)
{
#define   LEFT_KEY_NUM		(((NON_LEAF_KEY_NUM)/2))
#define   RIGHT_KEY_NUM		((NON_LEAF_KEY_NUM) - LEFT_KEY_NUM)

    // 2. search nonleaf nodes 
    Bnode* p;
    Bnode* parent;
    int oldver;
    Bnode* newp;
    int m, t, r, b, n, i, lev;
    key_type tr;
    int total_level;
    lev = 1;
restart:
    total_level = tree_meta->root_level;
    p = tree_meta->tree_root;
    while (lev <= total_level)
    {
        p = tree_meta->tree_root;
        for (i = tree_meta->root_level; i > lev; i--)
        {
            // if the lock bit is set, retry
	        parent = p;
            oldver = p->getlockver();
            if (p->getlock()) { goto restart; }
            b = 0; t = p->num-1;
            while (b + 7 <= t) {
                m = (b + t) >> 1;
                tr = key - p->k(m);
                if (tr > 0) b = m + 1;
                else if (tr < 0) t = m - 1;
                else { p = p->ch(m);  goto inner_done; }
            }
            // sequential search
            for (; b <= t; b++)
            {
                if (key < p->k(b)) break;
            }
            if (b == 0)
            {
                p = p->left_sibling;
            }
            else
            {
                p = p->ch(b - 1);
            }
        inner_done:
	    if (oldver != parent->getlockver())
            {
                goto restart;
            }
        }
        if (!p->bwlock()) { goto restart; }
        /* node is no full */
        //simply insert
        if (p->num < NON_LEAF_KEY_NUM)
        {
            //insert to inner of node
            int mk = p->num-1;
            for (; mk >= 0; mk--)
            {
                if (key > p->k(mk))
                {
                    break;
                }
                p->ent[mk + 1] = p->ent[mk];
            }
            p->k(mk + 1) = key;
            p->ch(mk + 1) = pt;
            p->num = p->num + 1;
	    
            //sfence();
            p->bwunlock();
            return;
        }
        /* node is full */
        else {
		//printf("bnode split %lld\n",p->num);
            key_type ckey;
            newp = (Bnode*)Ialloc(NONLEAF_SIZE);
	    newp->initlock();
            /* if key should be in the left node */
            if (key < p->k(LEFT_KEY_NUM - 1)) 
            {
                r = RIGHT_KEY_NUM - 1;
                i = NON_LEAF_KEY_NUM - 1;
                for (; r >= 0; r--, i--) {
                    newp->ent[r] = p->ent[i];
                }
                //ckey will push up
                ckey = p->k(i);
                newp->left_sibling = p->ch(i);                
                int j = i - 1;
                for (; p->k(j) > key && j >= 0; j--)
                {
                    p->ent[j + 1] = p->ent[j];
                }
                p->k(j + 1) = key; p->ch(j + 1) = pt;
            }
            /* if key should be in the right node */
            else {
                r = RIGHT_KEY_NUM - 1;
                i = NON_LEAF_KEY_NUM - 1;
                for (; p->k(i) > key && r >= 0; i--, r--) {
                    newp->ent[r] = p->ent[i];
                }
                if (r >= 0) {
                    newp->k(r) = key; newp->ch(r) = pt; r--;
                    for (; r >= 0; i--, r--) {
                        newp->ent[r] = p->ent[i];
                    }
                    ckey = p->k(i);
                    newp->left_sibling = p->ch(i);
                }
                else
                {
                    ckey = key;
                    newp->left_sibling = pt;
                }
                
            }
	    
            key = ckey; pt = newp;
            p->num = LEFT_KEY_NUM;
            newp->num = RIGHT_KEY_NUM;
            if (lev < total_level) p->bwunlock();
            lev++;
        }
    }
    //root is split
    newp = (Bnode*)Ialloc(NONLEAF_SIZE);
    newp->initlock();
    newp->bwlock();
    newp->left_sibling = tree_meta->tree_root; newp->ent[0].pv = pt; newp->ent[0].key = key;
    newp->num ++;
    //sfence();
    //alter the root node
    tree_meta->tree_root = newp;
    tree_meta->root_level = tree_meta->root_level + 1;
    sfence();
    if (total_level > 0) {
        p->bwunlock();
    }
    newp->bwunlock();
    return;
#undef RIGHT_KEY_NUM
#undef LEFT_KEY_NUM
}

/* ---------------------------------------------------------- *

 deletion

 lazy delete - insertions >= deletions in most cases
 so no need to change the tree structure frequently

 So unless there is no key in a leaf or no child in a non-leaf,
 the leaf and non-leaf won't be deleted.

 * ---------------------------------------------------------- */
bool btree::del(key_type key)
{

    unsigned char key_hash = hashcode1B(key);
    Bnode* p;
    Lnode* lp;
    int i, t, m, b;
    Bnode* parent;
    int oldver;
    key_type r;
restart:
    // 1. search nonleaf nodes
    p = tree_meta->tree_root;
    for (i = tree_meta->root_level; i > 0; i--) {
        // if the lock bit is set, retry
        parent = p;
        oldver = p->getlockver();
        if (p->getlock()) { goto restart; }
        b = 0; t = p->num-1;
        while (b + 7 <= t) {
            m = (b + t) >> 1;
            r = key - p->k(m);
            if (r > 0) b = m + 1;
            else if (r < 0) t = m - 1;
            else { p = p->ch(m); goto inner_done; }
        }
        for (; b <= t; b++)
        {
            if (key < p->k(b)) break;
        }
        if (b == 0)
        {
            p = p->left_sibling;
        }
        else
        {
            p = p->ch(b - 1);
        }        
    inner_done:
        if (oldver != parent->getlockver())
        {
            goto restart;
        }
    }
    // 3. search leaf node
    lp = (Lnode*)p;
    //find the node of deleted key
    while ((key > lp->getmaxkey()) && (lp->next != NULL))
    {
        lp = lp->next;
        
    }
//printf("del0\n");
    // if the lock bit is set, retry
    if (!lp->writelock()) { goto restart; }
    //printf("del\n");
    // search every matching candidate
    i = -1;    
    uint16_t fbitmap = lp->head.bitmap;
    while (fbitmap) {
        int jj = bitScan(fbitmap) - 1;  // next candidate
	if(lp->head.fingerPrint[jj] == key_hash)
        if (lp->k(jj) == key) { // found: good
            i = jj;
            break;
        }
        fbitmap &= ~(0x1 << jj);  // remove this bit
    } // end while
    if (i < 0) { lp->writeunlock(); return true; }

    /* Part 2. leaf node */
    /* 1. leaf contains more than one key */
    if (lp->num() > 1)
    {
        lp->head.bitmap &= ~(1 << i);
        clwb(lp); sfence();
        //set read version
        lp->setreadversion();
        lp->writeunlock();
        return true;
    } // end of more than one key

    /* 2. leaf has only one key: remove the leaf node */
    else 
    {
        lp->head.bitmap &= ~(1 << i);
        //set read version
        lp->setreadversion();
        if (!lp->head.hangin && tree_meta->root_level > 0) delBnode(key);
        //is first leaf node,modify the first leaf
        while (lp == (tree_meta->first_leaf) && lp->next != NULL)
        {
            //release lp->next node,if deleted is 1
            if (lp->next->writelock()) {
                if (lp->next->head.deleted)
                {
                    //lp->next = lp->next->next;
                    //lp->next->writeunlock();
                   // clwb(lp); sfence();
		   Lnode* tempt;
		   tempt = lp->next;
                    lp->next = tempt->next;
                    tempt->writeunlock();
                    
                }
                else 
                {
                    if (lp->next->head.hangin)
                    {
                        Update_Bnode(lp->next->getminkey(), lp->next);
                    }
                    tree_meta->setFirstLeaf(lp->next);
                    lp->next->writeunlock();                    
                    return true;
                }
            }
        }

        //not the first leaf node
        //next is deleted node, release the lp->next node
        while (lp->next != NULL && lp->next->head.deleted)
        {
            if (lp->next->writelock()) {
                //lp->next = lp->next->next;
                //lp->next->writeunlock();
                //clwb(lp); sfence();     
		Lnode* tempt;
                tempt = lp->next;
                lp->next = tempt->next;
                tempt->writeunlock();
            }
        }

        //set hangin node
        lp->head.deleted = 1;
        lp->head.hangin = 1;
        clwb(lp); sfence();
        lp->writeunlock();
        return true;
    }
    // end of Part 2
}

void btree::scan(key_type start_key, key_type end_key, long len)
{
    /* Part 1. get the positions to the start key */
    Bnode* p;
    Lnode* lp;
    unsigned long buf[200];
    int i, t, m, b;
    Bnode* parent;
    int oldver;
    key_type r;
restart:
    // 1. search nonleaf nodes
    p = tree_meta->tree_root;
    for (i = tree_meta->root_level; i > 0; i--) {
        // if the lock bit is set,retry
        parent = p;
        oldver = p->getlockver();
        if (p->getlock()) { goto restart; }
        b = 0; t = p->num-1;
        while (b + 7 <= t) {
            m = (b + t) >> 1;
            r = start_key - p->k(m);
            if (r > 0) b = m + 1;
            else if (r < 0) t = m - 1;
            else { p = p->ch(m); goto inner_done; }
        }
        for (; b <= t; b++)
        {
            if (start_key < p->k(b)) break;
        }
        if (b == 0)
        {
            p = p->left_sibling;
        }
        else
        {
            p = p->ch(b - 1);
        }
    inner_done:
        if (oldver != parent->getlockver())
        {
            goto restart;
        }
    }

    // 3. search leaf node
    lp = (Lnode*)p;
    //look for the all key from start_key to end_key
    int off = 0;
    while (lp != NULL)
    {
        int cont = off;
        bool order = false;
        int count = cont + lp->num();
        version_t vlock = lp->getreadversion();
        if (count > len || off == 0) { order = true; }
        uint16_t node_bitmap = lp->head.bitmap;
        node_bitmap &= 0x1fff;
        if (order)
        {
            int scount = 1;
            long long slot[16] = { 0 };
            while (node_bitmap)
            {
                int kh = bitScan(node_bitmap) - 1;
		
                for (int jf = scount; jf > 0; jf--)
                {
                    if (lp->k(kh) < slot[jf - 1])
                    {
                        slot[jf] = slot[jf - 1];
                    }
                    else {
                        slot[jf] = lp->ch(kh);
			scount++;
			break;
                    }
                }
                node_bitmap &= ~(0x1 << kh);
            }
            int mt = 1;
            for(; mt<scount; mt++) 
	    { 
		    if(cont < len && slot[mt] <= end_key && slot[mt] >= start_key){
		    buf[cont++] = slot[mt]; 
		    }
	    }
            if (lp->getreadversion() != vlock) { continue; }
            off = cont;
            if (off >= len) { return; }
            lp = lp->next;
        }
        else {
            while (node_bitmap)
            {
                int kh = bitScan(node_bitmap) - 1;
		buf[cont++]=lp->ch(kh);
                node_bitmap &= ~(0x1 << kh);
            }
            if (lp->getreadversion() != vlock) { continue; }
            off = cont;
            lp = lp->next;
        }
    }
    return;
}


void btree::delBnode(key_type key)
{
    Bnode* p;
    int  m, t, b, n, i, lev;
    key_type r;
    Bnode* parent;
    int oldver;
    lev = 1;
    // 2. search nonleaf nodes
restart:
    while (1)
    {
        p = tree_meta->tree_root;
        for (i = tree_meta->root_level; i > lev; i--)
        {
            // if the lock bit is set, retry
            parent = p;
            oldver = p->getlockver();
            if (p->getlock()) { goto restart; }
            b = 0; t = p->num-1;
            while (b + 7 <= t) {
                m = (b + t) >> 1;
                r = key - p->k(m);
                if (r > 0) b = m + 1;
                else if (r < 0) t = m - 1;
                else { p = p->ch(m); goto inner_done; }
            }
            // sequential search
            for (; b <= t; b++)
            {
                if (key < p->k(b)) break;
            }
            if (b == 0)
            {
                p = p->left_sibling;
            }
            else
            {
                p = p->ch(b - 1);
            }
        inner_done:
            if (oldver != parent->getlockver())
            {
                goto restart;
            }        
        }
        //del key from non-leaf node
        if (!p->bwlock()) { goto restart; }
        int fk = 0;
        n = p->num;
        //more than 1 key
        if (n > 0)
        {
            for (;fk < n; fk++)
            {
                if (key < p->k(fk))
                {
                    for (; fk < n; fk++)
                    {
                        if (fk == 0) { p->left_sibling = p->ch(0); }
                        else { p->ent[fk - 1] = p->ent[fk]; }
                    }
                    break;
                }
            }
            p->num = n - 1;
            sfence();
            if (p->num == 0 && lev >= tree_meta->root_level)break;
            p->bwunlock();
            return;
        }
        /* otherwise only 1 ptr */
        lev++;
    }
    // root has 1 child, delete the root
    tree_meta->root_level = tree_meta->root_level - 1;
    tree_meta->tree_root = p->ch(0);
    p->bwunlock();
    sfence();
    return;
}

/* ----------------------------------------------------------------- *
 print
 * ----------------------------------------------------------------- */
void btree::print()
{
    int num = 0;
    Lnode* lp;
    lp = (tree_meta->first_leaf);
    while (lp != NULL)
    {
	    
        if (lp->head.hangin)
        {
            lp->head.hangin = 0;
            Update_Bnode(lp->getminkey(), lp);
            num++;
        }
	
	    /*
	    uint64_t key=9105318085603802964;
         unsigned char key_hash = hashcode1B(key);
	
        uint16_t fbitmap = lp->head.bitmap;
	printf("%lld, %lld\n",lp->getminkey(),lp->getmaxkey());
	if(lp->getmaxkey()==key)
    while (fbitmap) {
        int jj = bitScan(fbitmap) - 1;  // next candidate
        if(lp->head.fingerPrint[jj] == key_hash)
        if (lp->k(jj) == key) { // found
            //printf("%lld\n",lp->k(jj));
            break;
        }
	printf("%lld\n",lp->k(jj));
        fbitmap &= ~(0x1 << jj);  // remove this bit
    } // end while
printf("\n");
*/
	/*    
	    if(lp->next != NULL){
	if(lp->getminkey() > lp->next->Min_Key)
	{err++;
		printf("the hangin status is %ld,%ld\n",lp->head.hangin,lp->next->head.hangin);
	}
	    }
	    
	    //lp->getminkey();
	num=num+lp->num();
	*/
        lp = lp->next;
    }
    printf("the hangin node is %ld\n",num);
    return;
}


/* ----------------------------------------------------------------- *
 check structure integrity
 * ----------------------------------------------------------------- */



void btree::checkFirstLeaf(void)
{
    // get left-most leaf node
    Bnode* p = tree_meta->tree_root;
    for (int i = tree_meta->root_level; i > 0; i--) p = p->left_sibling;

    if ((Lnode*)p != (tree_meta->first_leaf)) {
        printf("first leaf %p != %p\n", tree_meta->first_leaf, p);
        exit(1);
    }
}

