struct HNode {
    HNode *next = NULL;
    uint64_t hcode = 0; // hash value
};

struct HTab {
    HNode **tab = NULL; // array of slots
    size_t mask = 0;    // power of 2 array size, 2^n - 1
    size_t size = 0;    // number of keys
};

static void h_init(HTab *htab, size_t n) {
    assert(n > 0 && ((n - 1) & n) == 0);    // n must be a power of 2
    htab->tab = (HNode **)calloc(n, sizeof(HNode *));
    htab->mask = n - 1;
    htab->size = 0;
}

static void h_insert(HTab *htab, HNode *node) {
    size_t pos = node->hcode & htab->mask;  // node->hcode & (n - 1)
    HNode *next = htab->tab[pos];
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (!htab->tab) {
        return NULL;
    }
    size_t pos = key->hcode & htab->mask;
    HNode **from = &htab->tab[pos];     // incoming pointer to the target
    for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
        if (cur->hcode == key->hcode && eq(cur, key)) {
            return from;    // Q: Why not return `cur`? This is for deletion.
        }
    }
    return NULL;
}

static HNode *h_detach(HTab *htab, HNode **from) {
    HNode *node = *from;    // the target node
    *from = node->next;     // update the incoming pointer to the target
    htab->size--;
    return node;
}

struct HMap {
    HTab newer;
    HTab older;
    size_t migrate_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void   hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));

static void hm_trigger_rehashing(HMap *hmap) {
    hmap->older = hmap->newer;  // (newer, older) <- (new_table, newer)
    h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
    hmap->migrate_pos = 0;
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    HNode **from = h_lookup(&hmap->newer, key, eq);
    if (!from) {
        from = h_lookup(&hmap->older, key, eq);
    }
    return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
    if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
        return h_detach(&hmap->newer, from);
    }
    if (HNode **from = h_lookup(&hmap->older, key, eq)) {
        return h_detach(&hmap->older, from);
    }
    return NULL;
}

void hm_insert(HMap *hmap, HNode *node) {
    if (!hmap->newer.tab) {
        h_init(&hmap->newer, 4);    // initialized it if empty
    }
    h_insert(&hmap->newer, node);   // always insert to the newer table
    if (!hmap->older.tab) {         // check whether we need to rehash
        size_t shreshold = (hmap->newer.mask + 1) * k_max_load_factor;
        if (hmap->newer.size >= shreshold) {
            hm_trigger_rehashing(hmap);
        }
    }
    hm_help_rehashing(hmap);        // migrate some keys
}

const size_t k_max_load_factor = 8;

const size_t k_rehashing_work = 128;    // constant work

static void hm_help_rehashing(HMap *hmap) {
    size_t nwork = 0;
    while (nwork < k_rehashing_work && hmap->older.size > 0) {
        // find a non-empty slot
        HNode **from = &hmap->older.tab[hmap->migrate_pos];
        if (!*from) {
            hmap->migrate_pos++;
            continue;   // empty slot
        }
        // move the first list item to the newer table
        h_insert(&hmap->newer, h_detach(&hmap->older, from));
        nwork++;
    }
    // discard the old table if done
    if (hmap->older.size == 0 && hmap->older.tab) {
        free(hmap->older.tab);
        hmap->older = HTab{};
    }
}

static struct {
    HMap db;    // top-level hashtable
} g_data;

// KV pair for the top-level hashtable
struct Entry {
    struct HNode node;  // hashtable node
    std::string key;
    std::string val;
};

static bool entry_eq(HNode *lhs, HNode *rhs) {
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}


static void do_get(std::vector<std::string> &cmd, Response &out) {
    // a dummy `Entry` just for the lookup
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    // hashtable lookup
    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node) {
        out.status = RES_NX;
        return;
    }
    // copy the value
    const std::string &val = container_of(node, Entry, node)->val;
    assert(val.size() <= k_max_msg);
    out.data.assign(val.begin(), val.end());
}