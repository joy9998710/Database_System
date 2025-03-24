#include "bpt.h"

H_P * hp[2];

page * rt[2] = {NULL, NULL}; //root is declared as global

int fd[2] = {-1, -1}; //fd is declared as global


H_P * load_header(int table_id) {
    H_P * newhp = (H_P*)calloc(1, sizeof(H_P));
    if (sizeof(H_P) > pread(fd[table_id], newhp, sizeof(H_P), 0)) {

        return NULL;
    }
    return newhp;
}


page * load_page(off_t off, int table_id) {
    page* load = (page*)calloc(1, sizeof(page));
    //if (off % sizeof(page) != 0) printf("load fail : page offset error\n");
    if (sizeof(page) > pread(fd[table_id], load, sizeof(page), off)) {

        return NULL;
    }
    return load;
}

int open_table(char * pathname, int table_id) {
    fd[table_id] = open(pathname, O_RDWR | O_CREAT | O_EXCL | O_SYNC  , 0775);
    hp[table_id] = (H_P *)calloc(1, sizeof(H_P));
    if (fd[table_id] > 0) {
        //printf("New File created\n");
        hp[table_id]->fpo = 0;
        hp[table_id]->num_of_pages = 1;
        hp[table_id]->rpo = 0;
        pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
        free(hp[table_id]);
        hp[table_id] = load_header(table_id);
        return 0;
    }
    fd[table_id] = open(pathname, O_RDWR|O_SYNC);
    if (fd[table_id] > 0) {
        //printf("Read Existed File\n");
        if (sizeof(H_P) > pread(fd[table_id], hp[table_id], sizeof(H_P), 0)) {
            return -1;
        }
        off_t r_o = hp[table_id]->rpo;
        rt[table_id] = load_page(r_o, table_id);
        return 0;
    }
    else return -1;
}

void reset(off_t off, int table_id) {
    page * reset;
    reset = (page*)calloc(1, sizeof(page));
    reset->parent_page_offset = 0;
    reset->is_leaf = 0;
    reset->num_of_keys = 0;
    reset->next_offset = 0;
    pwrite(fd[table_id], reset, sizeof(page), off);
    free(reset);
    return;
}

void freetouse(off_t fpo, int table_id) {
    page * reset;
    reset = load_page(fpo, table_id);
    reset->parent_page_offset = 0;
    reset->is_leaf = 0;
    reset->num_of_keys = 0;
    reset->next_offset = 0;
    pwrite(fd[table_id], reset, sizeof(page), fpo);
    free(reset);
    return;
}

void usetofree(off_t wbf, int table_id) {
    page * utf = load_page(wbf, table_id);
    utf->parent_page_offset = hp[table_id]->fpo;
    utf->is_leaf = 0;
    utf->num_of_keys = 0;
    utf->next_offset = 0;
    pwrite(fd[table_id], utf, sizeof(page), wbf);
    free(utf);
    hp[table_id]->fpo = wbf;
    pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
    free(hp[table_id]);
    hp[table_id] = load_header(table_id);
    return;
}

off_t new_page(int table_id) {
    off_t newp;
    page * np;
    off_t prev;
    if (hp[table_id]->fpo != 0) {
        newp = hp[table_id]->fpo;
        np = load_page(newp, table_id);
        hp[table_id]->fpo = np->parent_page_offset;
        pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
        free(hp[table_id]);
        hp[table_id] = load_header(table_id);
        free(np);
        freetouse(newp, table_id);
        return newp;
    }
    //change previous offset to 0 is needed
    newp = lseek(fd[table_id], 0, SEEK_END);
    //if (newp % sizeof(page) != 0) printf("new page made error : file size error\n");
    reset(newp, table_id);
    hp[table_id]->num_of_pages++;
    pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
    free(hp[table_id]);
    hp[table_id] = load_header(table_id);
    return newp;
}

off_t find_leaf(int64_t key, int table_id) {
    int i = 0;
    page * p;
    off_t loc = hp[table_id]->rpo;

    //printf("left = %ld, key = %ld, right = %ld, is_leaf = %d, now_root = %ld\n", rt->next_offset, 
    //  rt->b_f[0].key, rt->b_f[0].p_offset, rt->is_leaf, hp->rpo);

    if (rt[table_id] == NULL) {
        //printf("Empty tree.\n");
        return 0;
    }
    p = load_page(loc, table_id);

    while (!p->is_leaf) {
        i = 0;

        while (i < p->num_of_keys) {
            if (key >= p->b_f[i].key) i++;
            else break;
        }
        if (i == 0) loc = p->next_offset;
        else
            loc = p->b_f[i - 1].p_offset;
        //if (loc == 0)
        // return NULL;

        free(p);
        p = load_page(loc, table_id);

    }

    free(p);
    return loc;
}


char * db_find(int64_t key, int table_id) {
    char * value = (char*)malloc(sizeof(char) * 120);
    int i = 0;
    off_t fin = find_leaf(key, table_id);
    if (fin == 0) {
        return NULL;
    }
    page * p = load_page(fin, table_id);

    for (; i < p->num_of_keys; i++) {
        if (p->records[i].key == key) break;
    }
    if (i == p->num_of_keys) {
        free(p);
        return NULL;
    }
    else {
        strcpy(value, p->records[i].value);
        free(p);
        return value;
    }
}

int cut(int length) {
    if (length % 2 == 0)
        return length / 2;
    else
        return length / 2 + 1;
}



void start_new_file(record rec, int table_id) {

    page * root;
    off_t ro;
    ro = new_page(table_id);
    rt[table_id] = load_page(ro, table_id);
    hp[table_id]->rpo = ro;
    pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
    free(hp[table_id]);
    hp[table_id] = load_header(table_id);
    rt[table_id]->num_of_keys = 1;
    rt[table_id]->is_leaf = 1;
    rt[table_id]->records[0] = rec;
    pwrite(fd[table_id], rt[table_id], sizeof(page), hp[table_id]->rpo);
    free(rt[table_id]);
    rt[table_id] = load_page(hp[table_id]->rpo, table_id);
    //printf("new file is made\n");
}

int db_insert(int64_t key, char * value, int table_id) {

    record nr;
    nr.key = key;
    strcpy(nr.value, value);
    if (rt[table_id] == NULL) {
        start_new_file(nr, table_id);
        return 0;
    }

    char * dupcheck;
    dupcheck = db_find(key, table_id);
    if (dupcheck != NULL) {
        free(dupcheck);
        return -1;
    }
    free(dupcheck);

    off_t leaf = find_leaf(key, table_id);

    page * leafp = load_page(leaf, table_id);

    if (leafp->num_of_keys < LEAF_MAX) {
        insert_into_leaf(leaf, nr, table_id);
        free(leafp);
        return 0;
    }

    insert_into_leaf_as(leaf, nr, table_id);
    free(leafp);
    //why double free?
    return 0;

}

off_t insert_into_leaf(off_t leaf, record inst, int table_id) {

    page * p = load_page(leaf, table_id);
    //if (p->is_leaf == 0) printf("iil error : it is not leaf page\n");
    int i, insertion_point;
    insertion_point = 0;
    while (insertion_point < p->num_of_keys && p->records[insertion_point].key < inst.key) {
        insertion_point++;
    }
    for (i = p->num_of_keys; i > insertion_point; i--) {
        p->records[i] = p->records[i - 1];
    }
    p->records[insertion_point] = inst;
    p->num_of_keys++;
    pwrite(fd[table_id], p, sizeof(page), leaf);
    //printf("insertion %ld is complete %d, %ld\n", inst.key, p->num_of_keys, leaf);
    free(p);
    return leaf;
}


off_t insert_into_leaf_as(off_t leaf, record inst, int table_id) {
    off_t new_leaf;
    record * temp;
    int insertion_index, split, i, j;
    int64_t new_key;
    new_leaf = new_page(table_id);
    //printf("\n%ld is new_leaf offset\n\n", new_leaf);
    page * nl = load_page(new_leaf, table_id);
    nl->is_leaf = 1;
    temp = (record *)calloc(LEAF_MAX + 1, sizeof(record));
    if (temp == NULL) {
        perror("Temporary records array");
        exit(EXIT_FAILURE);
    }
    insertion_index = 0;
    page * ol = load_page(leaf, table_id);
    while (insertion_index < LEAF_MAX && ol->records[insertion_index].key < inst.key) {
        insertion_index++;
    }
    for (i = 0, j = 0; i < ol->num_of_keys; i++, j++) {
        if (j == insertion_index) j++;
        temp[j] = ol->records[i];
    }
    temp[insertion_index] = inst;
    ol->num_of_keys = 0;
    split = cut(LEAF_MAX);

    for (i = 0; i < split; i++) {
        ol->records[i] = temp[i];
        ol->num_of_keys++;
    }

    for (i = split, j = 0; i < LEAF_MAX + 1; i++, j++) {
        nl->records[j] = temp[i];
        nl->num_of_keys++;
    }

    free(temp);

    nl->next_offset = ol->next_offset;
    ol->next_offset = new_leaf;

    for (i = ol->num_of_keys; i < LEAF_MAX; i++) {
        ol->records[i].key = 0;
        //strcpy(ol->records[i].value, NULL);
    }

    for (i = nl->num_of_keys; i < LEAF_MAX; i++) {
        nl->records[i].key = 0;
        //strcpy(nl->records[i].value, NULL);
    }

    nl->parent_page_offset = ol->parent_page_offset;
    new_key = nl->records[0].key;

    pwrite(fd[table_id], nl, sizeof(page), new_leaf);
    pwrite(fd[table_id], ol, sizeof(page), leaf);
    free(ol);
    free(nl);
    //printf("split_leaf is complete\n");

    return insert_into_parent(leaf, new_key, new_leaf, table_id);

}

off_t insert_into_parent(off_t old, int64_t key, off_t newp, int table_id) {

    int left_index;
    off_t bumo;
    page * left;
    left = load_page(old, table_id);

    bumo = left->parent_page_offset;
    free(left);

    if (bumo == 0)
        return insert_into_new_root(old, key, newp, table_id);

    left_index = get_left_index(old, table_id);

    page * parent = load_page(bumo, table_id);
    //printf("\nbumo is %ld\n", bumo);
    if (parent->num_of_keys < INTERNAL_MAX) {
        free(parent);
        //printf("\nuntil here is ok\n");
        return insert_into_internal(bumo, left_index, key, newp, table_id);
    }
    free(parent);
    return insert_into_internal_as(bumo, left_index, key, newp, table_id);
}

int get_left_index(off_t left, int table_id) {
    page * child = load_page(left, table_id);
    off_t po = child->parent_page_offset;
    free(child);
    page * parent = load_page(po, table_id);
    int i = 0;
    if (left == parent->next_offset) return -1;
    for (; i < parent->num_of_keys; i++) {
        if (parent->b_f[i].p_offset == left) break;
    }

    if (i == parent->num_of_keys) {
        free(parent);
        return -10;
    }
    free(parent);
    return i;
}

off_t insert_into_new_root(off_t old, int64_t key, off_t newp, int table_id) {

    off_t new_root;
    new_root = new_page(table_id);
    page * nr = load_page(new_root, table_id);
    nr->b_f[0].key = key;
    nr->next_offset = old;
    nr->b_f[0].p_offset = newp;
    nr->num_of_keys++;
    //printf("key = %ld, old = %ld, new = %ld, nok = %d, nr = %ld\n", key, old, newp, 
    //  nr->num_of_keys, new_root);
    page * left = load_page(old, table_id);
    page * right = load_page(newp, table_id);
    left->parent_page_offset = new_root;
    right->parent_page_offset = new_root;
    pwrite(fd[table_id], nr, sizeof(page), new_root);
    pwrite(fd[table_id], left, sizeof(page), old);
    pwrite(fd[table_id], right, sizeof(page), newp);
    free(nr);
    nr = load_page(new_root, table_id);
    rt[table_id] = nr;
    hp[table_id]->rpo = new_root;
    pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
    free(hp[table_id]);
    hp[table_id] = load_header(table_id);
    free(left);
    free(right);
    return new_root;

}

off_t insert_into_internal(off_t bumo, int left_index, int64_t key, off_t newp, int table_id) {

    page * parent = load_page(bumo, table_id);
    int i;

    for (i = parent->num_of_keys; i > left_index + 1; i--) {
        parent->b_f[i] = parent->b_f[i - 1];
    }
    parent->b_f[left_index + 1].key = key;
    parent->b_f[left_index + 1].p_offset = newp;
    parent->num_of_keys++;
    pwrite(fd[table_id], parent, sizeof(page), bumo);
    free(parent);
    if (bumo == hp[table_id]->rpo) {
        free(rt[table_id]);
        rt[table_id] = load_page(bumo, table_id);
        //printf("\nrt->numofkeys%lld\n", rt->num_of_keys);

    }
    return hp[table_id]->rpo;
}

off_t insert_into_internal_as(off_t bumo, int left_index, int64_t key, off_t newp, int table_id) {

    int i, j, split;
    int64_t k_prime;
    off_t new_p, child;
    I_R * temp;

    temp = (I_R *)calloc(INTERNAL_MAX + 1, sizeof(I_R));

    page * old_parent = load_page(bumo, table_id);

    for (i = 0, j = 0; i < old_parent->num_of_keys; i++, j++) {
        if (j == left_index + 1) j++;
        temp[j] = old_parent->b_f[i];
    }

    temp[left_index + 1].key = key;
    temp[left_index + 1].p_offset = newp;

    split = cut(INTERNAL_MAX);
    new_p = new_page(table_id);
    page * new_parent = load_page(new_p, table_id);
    old_parent->num_of_keys = 0;
    for (i = 0; i < split; i++) {
        old_parent->b_f[i] = temp[i];
        old_parent->num_of_keys++;
    }
    k_prime = temp[i].key;
    new_parent->next_offset = temp[i].p_offset;
    for (++i, j = 0; i < INTERNAL_MAX + 1; i++, j++) {
        new_parent->b_f[j] = temp[i];
        new_parent->num_of_keys++;
    }

    new_parent->parent_page_offset = old_parent->parent_page_offset;
    page * nn;
    nn = load_page(new_parent->next_offset, table_id);
    nn->parent_page_offset = new_p;
    pwrite(fd[table_id], nn, sizeof(page), new_parent->next_offset);
    free(nn);
    for (i = 0; i < new_parent->num_of_keys; i++) {
        child = new_parent->b_f[i].p_offset;
        page * ch = load_page(child, table_id);
        ch->parent_page_offset = new_p;
        pwrite(fd[table_id], ch, sizeof(page), child);
        free(ch);
    }

    pwrite(fd[table_id], old_parent, sizeof(page), bumo);
    pwrite(fd[table_id], new_parent, sizeof(page), new_p);
    free(old_parent);
    free(new_parent);
    free(temp);
    //printf("split internal is complete\n");
    return insert_into_parent(bumo, k_prime, new_p, table_id);
}

int db_delete(int64_t key, int table_id) {

    if (rt[table_id]->num_of_keys == 0) {
        //printf("root is empty\n");
        return -1;
    }
    char * check = db_find(key, table_id);
    if (check== NULL) {
        free(check);
        //printf("There are no key to delete\n");
        return -1;
    }
    free(check);
    off_t deloff = find_leaf(key, table_id);
    delete_entry(key, deloff, table_id);
    return 0;

}//fin

void delete_entry(int64_t key, off_t deloff, int table_id) {

    remove_entry_from_page(key, deloff, table_id);

    if (deloff == hp[table_id]->rpo) {
        adjust_root(deloff, table_id);
        return;
    }
    page * not_enough = load_page(deloff, table_id);
    int check = not_enough->is_leaf ? cut(LEAF_MAX) : cut(INTERNAL_MAX);
    if (not_enough->num_of_keys >= check){
      free(not_enough);
      //printf("just delete\n");
      return;  
    } 

    int neighbor_index, k_prime_index;
    off_t neighbor_offset, parent_offset;
    int64_t k_prime;
    parent_offset = not_enough->parent_page_offset;
    page * parent = load_page(parent_offset, table_id);

    if (parent->next_offset == deloff) {
        neighbor_index = -2;
        neighbor_offset = parent->b_f[0].p_offset;
        k_prime = parent->b_f[0].key;
        k_prime_index = 0;
    }
    else if(parent->b_f[0].p_offset == deloff) {
        neighbor_index = -1;
        neighbor_offset = parent->next_offset;
        k_prime_index = 0;
        k_prime = parent->b_f[0].key;
    }
    else {
        int i;

        for (i = 0; i <= parent->num_of_keys; i++)
            if (parent->b_f[i].p_offset == deloff) break;
        neighbor_index = i - 1;
        neighbor_offset = parent->b_f[i - 1].p_offset;
        k_prime_index = i;
        k_prime = parent->b_f[i].key;
    }

    page * neighbor = load_page(neighbor_offset, table_id);
    int max = not_enough->is_leaf ? LEAF_MAX : INTERNAL_MAX - 1;
    int why = neighbor->num_of_keys + not_enough->num_of_keys;
    //printf("%d %d\n",why, max);
    if (why <= max) {
        free(not_enough);
        free(parent);
        free(neighbor);
        coalesce_pages(deloff, neighbor_index, neighbor_offset, parent_offset, k_prime, table_id);
    }
    else {
        free(not_enough);
        free(parent);
        free(neighbor);
        redistribute_pages(deloff, neighbor_index, neighbor_offset, parent_offset, k_prime, k_prime_index, table_id);

    }

    return;

}
void redistribute_pages(off_t need_more, int nbor_index, off_t nbor_off, off_t par_off, int64_t k_prime, int k_prime_index, int table_id) {
    
    page *need, *nbor, *parent;
    int i;
    need = load_page(need_more, table_id);
    nbor = load_page(nbor_off, table_id);
    parent = load_page(par_off, table_id);
    if (nbor_index != -2) {
        
        if (!need->is_leaf) {
            //printf("redis average interal\n");
            for (i = need->num_of_keys; i > 0; i--)
                need->b_f[i] = need->b_f[i - 1];
            
            need->b_f[0].key = k_prime;
            need->b_f[0].p_offset = need->next_offset;
            need->next_offset = nbor->b_f[nbor->num_of_keys - 1].p_offset;
            page * child = load_page(need->next_offset, table_id);
            child->parent_page_offset = need_more;
            pwrite(fd[table_id], child, sizeof(page), need->next_offset);
            free(child);
            parent->b_f[k_prime_index].key = nbor->b_f[nbor->num_of_keys - 1].key;
            
        }
        else {
            //printf("redis average leaf\n");
            for (i = need->num_of_keys; i > 0; i--){
                need->records[i] = need->records[i - 1];
            }
            need->records[0] = nbor->records[nbor->num_of_keys - 1];
            nbor->records[nbor->num_of_keys - 1].key = 0;
            parent->b_f[k_prime_index].key = need->records[0].key;
        }

    }
    else {
        //
        if (need->is_leaf) {
            //printf("redis leftmost leaf\n");
            need->records[need->num_of_keys] = nbor->records[0];
            for (i = 0; i < nbor->num_of_keys - 1; i++)
                nbor->records[i] = nbor->records[i + 1];
            parent->b_f[k_prime_index].key = nbor->records[0].key;
            
           
        }
        else {
            //printf("redis leftmost internal\n");
            need->b_f[need->num_of_keys].key = k_prime;
            need->b_f[need->num_of_keys].p_offset = nbor->next_offset;
            page * child = load_page(need->b_f[need->num_of_keys].p_offset, table_id);
            child->parent_page_offset = need_more;
            pwrite(fd[table_id], child, sizeof(page), need->b_f[need->num_of_keys].p_offset);
            free(child);
            
            parent->b_f[k_prime_index].key = nbor->b_f[0].key;
            nbor->next_offset = nbor->b_f[0].p_offset;
            for (i = 0; i < nbor->num_of_keys - 1 ; i++)
                nbor->b_f[i] = nbor->b_f[i + 1];
            
        }
    }
    nbor->num_of_keys--;
    need->num_of_keys++;
    pwrite(fd[table_id], parent, sizeof(page), par_off);
    pwrite(fd[table_id], nbor, sizeof(page), nbor_off);
    pwrite(fd[table_id], need, sizeof(page), need_more);
    free(parent); free(nbor); free(need);
    return;
}

void coalesce_pages(off_t will_be_coal, int nbor_index, off_t nbor_off, off_t par_off, int64_t k_prime, int table_id) {
    
    page *wbc, *nbor, *parent;
    off_t newp, wbf;

    if (nbor_index == -2) {
        //printf("leftmost\n");
        wbc = load_page(nbor_off, table_id); nbor = load_page(will_be_coal, table_id); parent = load_page(par_off, table_id);
        newp = will_be_coal; wbf = nbor_off;
    }
    else {
        wbc = load_page(will_be_coal, table_id); nbor = load_page(nbor_off, table_id); parent = load_page(par_off, table_id);
        newp = nbor_off; wbf = will_be_coal;
    }

    int point = nbor->num_of_keys;
    int le = wbc->num_of_keys;
    int i, j;
    if (!wbc->is_leaf) {
        //printf("coal internal\n");
        nbor->b_f[point].key = k_prime;
        nbor->b_f[point].p_offset = wbc->next_offset;
        nbor->num_of_keys++;

        for (i = point + 1, j = 0; j < le; i++, j++) {
            nbor->b_f[i] = wbc->b_f[j];
            nbor->num_of_keys++;
            wbc->num_of_keys--;
        }

        for (i = point; i < nbor->num_of_keys; i++) {
            page * child = load_page(nbor->b_f[i].p_offset, table_id);
            child->parent_page_offset = newp;
            pwrite(fd[table_id], child, sizeof(page), nbor->b_f[i].p_offset);
            free(child);
        }

    }
    else {
        //printf("coal leaf\n");
        int range = wbc->num_of_keys;
        for (i = point, j = 0; j < range; i++, j++) {
            
            nbor->records[i] = wbc->records[j];
            nbor->num_of_keys++;
            wbc->num_of_keys--;
        }
        nbor->next_offset = wbc->next_offset;
    }
    pwrite(fd[table_id], nbor, sizeof(page), newp);
    
    delete_entry(k_prime, par_off, table_id);
    free(wbc);
    usetofree(wbf, table_id);
    free(nbor);
    free(parent);
    return;

}//fin

void adjust_root(off_t deloff, int table_id) {

    if (rt[table_id]->num_of_keys > 0)
        return;
    if (!rt[table_id]->is_leaf) {
        off_t nr = rt[table_id]->next_offset;
        page * nroot = load_page(nr, table_id);
        nroot->parent_page_offset = 0;
        usetofree(hp[table_id]->rpo, table_id);
        hp[table_id]->rpo = nr;
        pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
        free(hp[table_id]);
        hp[table_id] = load_header(table_id);
        
        pwrite(fd[table_id], nroot, sizeof(page), nr);
        free(nroot);
        free(rt[table_id]);
        rt[table_id] = load_page(nr, table_id);

        return;
    }
    else {
        free(rt[table_id]);
        rt[table_id] = NULL;
        usetofree(hp[table_id]->rpo, table_id);
        hp[table_id]->rpo = 0;
        pwrite(fd[table_id], hp[table_id], sizeof(H_P), 0);
        free(hp[table_id]);
        hp[table_id] = load_header(table_id);
        return;
    }
}//fin

void remove_entry_from_page(int64_t key, off_t deloff, int table_id) {
    
    int i = 0;
    page * lp = load_page(deloff, table_id);
    if (lp->is_leaf) {
        //printf("remove leaf key %ld\n", key);
        while (lp->records[i].key != key)
            i++;

        for (++i; i < lp->num_of_keys; i++)
            lp->records[i - 1] = lp->records[i];
        lp->num_of_keys--;
        pwrite(fd[table_id], lp, sizeof(page), deloff);
        if (deloff == hp[table_id]->rpo) {
            free(lp);
            free(rt[table_id]);
            rt[table_id] = load_page(deloff, table_id);
            return;
        }
        
        free(lp);
        return;
    }
    else {
        //printf("remove interanl key %ld\n", key);
        while (lp->b_f[i].key != key)
            i++;
        for (++i; i < lp->num_of_keys; i++)
            lp->b_f[i - 1] = lp->b_f[i];
        lp->num_of_keys--;
        pwrite(fd[table_id], lp, sizeof(page), deloff);
        if (deloff == hp[table_id]->rpo) {
            free(lp);
            free(rt[table_id]);
            rt[table_id] = load_page(deloff, table_id);
            return;
        }
        
        free(lp);
        return;
    }
    
}

// Assignment 4
//1GB memory available
//one page = 4KB
//2^30 / 4 * 2^10 = 262144 => let MAX_NUM_BLOCKS_PER_TABLE = 130000
void db_join(){
    //각 table에서 첫 leaf node 찾기
    off_t leaf_offset_0 = find_leaf(MIN_KEY, 0);
    off_t leaf_offset_1 = find_leaf(MIN_KEY, 1);

    page * page_0 = NULL;
    page * page_1 = NULL;

    int i_0 = 0;
    int i_1 = 0;

    //merge join
    while(leaf_offset_0 != 0 && leaf_offset_1 != 0){
        //페이지 끝까지 돌았을 경우 다음 leaf node load
        if(page_0 == NULL){
            page_0 = load_page(leaf_offset_0, 0);
            i_0 = 0;
        }
        if(page_1 == NULL){
            page_1 = load_page(leaf_offset_1, 1);
            i_1 = 0;
        }

        int64_t key_0 = page_0->records[i_0].key;
        int64_t key_1 = page_1->records[i_1].key;

        if(key_0 < key_1){
            //table_0의 key가 더 작으면 다음 키로 이동
            i_0++;
            //index최대치를 넘어가면
            if(i_0 >= page_0->num_of_keys){
                leaf_offset_0 = page_0->next_offset;
                free(page_0);
                page_0 = NULL;
            }
        }
        else if(key_0 > key_1){
            //table_1의 key가 더 작으면 다음 키로 이동
            i_1++;
            if(i_1 >= page_1->num_of_keys){
                leaf_offset_1 = page_1->next_offset;
                free(page_1);
                page_1 = NULL;
            }
        }
        else{
            //key가 서로 같은 경우
            char * value_0 = page_0->records[i_0].value;
            char * value_1 = page_1->records[i_1].value;
            printf("%ld,%s,%s\n", key_0, value_0, value_1);
            i_0++;
            i_1++;
            if(i_0 >= page_0->num_of_keys){
                leaf_offset_0 = page_0->next_offset;
                free(page_0);
                page_0 = NULL; 
            }
            if(i_1 >= page_1->num_of_keys){
                leaf_offset_1 = page_1->next_offset;
                free(page_1);
                page_1 = NULL;
            }
        }
    }
    //남은 page가 있다면 free 해주기
    if(page_0 != NULL){
        free(page_0);
    }
    if(page_1 != NULL){
        free(page_1);
    }
}
