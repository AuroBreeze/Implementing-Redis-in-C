#include "hashtable.h"
#include <cassert>
#include <cstdlib>
#include <cstddef>   // for size_t
#include <cstdint>   // for uint64_t

static void h_init(HTab* htab, size_t n){ // n must be power of 2
    assert(n > 0 && ((n-1) & n) == 0);
    htab->tab = (HNode* *)calloc(n,sizeof(HNode* ));
    htab->mask = n-1;
    htab->size = 0;
}

static void h_insert(HTab* htab,HNode* node){
    size_t pos = node->hcode & htab->mask;
    HNode* next = htab->tab[pos];
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

static HNode* *h_lookup(HTab* htab, HNode* key, bool (*eq)(HNode* , HNode*)){
    if(!htab->tab) return nullptr;
    size_t pos = key->hcode & htab->mask;
    HNode* *from = &htab->tab[pos];
    for(HNode* cur;(cur = *from) != nullptr;from = &cur->next){
        if(cur->hcode == key->hcode && eq(cur,key)) return from;
    }
    return nullptr;
}


//remove a node from chain
static HNode* h_detach(HTab* htab, HNode* *from){
    HNode* node = *from;
    *from = node->next;
    htab->size--;
    return node;
}

const size_t k_rehashing_work = 128; // costant work

static void hm_help_rehashing(HMap* hmap){
    size_t nwork = 0;
    while(nwork < k_rehashing_work && hmap->older.size > 0){
        HNode* *from = &hmap->older.tab[hmap->migrate_pos];
        if(!*from){
            hmap->migrate_pos++;
            continue;
        }
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        nwork++;

        if(hmap->older.size == 0 && hmap->older.tab){ // C 风格数组不会有“空数组就是 false”这种说法
            free(hmap->older.tab);
            hmap->older = HTab{};
        }
    }
}

static void hm_trigger_rehashing(HMap* hmap){
    assert(hmap->older.tab == nullptr);
    //(newer,older) <-- (new_table,newer)
    hmap->older = hmap->newer; // in the first time, the all data store in newer,we need to move to older first then rehashing newer
    h_init(&hmap->newer,(hmap->newer.mask+1)*2); // ensure the new table size is enough and the mask is power of 2
    hmap->migrate_pos = 0;

}

HNode* hm_lookup(HMap* hmap,HNode* key,bool (*eq)(HNode* , HNode*)){
    hm_help_rehashing(hmap);
    HNode* *from = h_lookup(&hmap->newer,key,eq); 
    if(!from){
        from = h_lookup(&hmap->older,key,eq);
    }
    return from ? *from : nullptr;
}

const size_t k_max_load_factor = 8;

void hm_insert(HMap* hmap,HNode* node){
    if(!hmap->newer.tab){
        h_init(&hmap->newer,4);
    }

    h_insert(&hmap->newer,node);

    if(!hmap->older.tab){ // check if we need to rehash
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if(hmap->newer.size >= shreshold){ 
            hm_trigger_rehashing(hmap);
        }
    }

    hm_help_rehashing(hmap); // migrate some nodes
}

HNode* hm_delete(HMap* hmap,HNode* key,bool (*eq)(HNode* , HNode*)){
    hm_help_rehashing(hmap);
    if(HNode* *from = h_lookup(&hmap->newer,key,eq)){
        return h_detach(&hmap->newer,from);
    }
    if(HNode* *from = h_lookup(&hmap->older,key,eq)){
        return h_detach(&hmap->older,from);
    }
    return nullptr;

}

void hm_clear(HMap* hmap){
    free(hmap->newer.tab);
    free(hmap->older.tab);
    *hmap = HMap{};
}

size_t hm_size(HMap* hmap){
    return hmap->newer.size + hmap->older.size;
}

static bool h_foreach(HTab* htab,bool (*f)(HNode*, void*),void* arg){
    for(size_t i=0; htab->mask !=0 && i<= htab->mask; ++i){
        for(HNode* node=htab->tab[i]; node != nullptr; node= node->next){
            if(!f(node, arg)){
                return false;
            }
        }
    }
    return true;
}

void hm_foreach(HMap* hmap, bool (*f)(HNode*, void*),void* arg){
    h_foreach(&hmap->newer, f, arg) && h_foreach(&hmap->older, f, arg);
}

