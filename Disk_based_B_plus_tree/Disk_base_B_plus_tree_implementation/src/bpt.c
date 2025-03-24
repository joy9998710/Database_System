#include "bpt.h"

//header page 선언
H_P * hp;

//root page를 나타냄
page * rt = NULL; //root is declared as global

//file descriptor로 파일 입출력을 할 때의 반환값 저장
int fd = -1; //fd is declared as global


//header page를 불러오는 함수 off : offset을 나타내는 변수지만 이 함수에서는 header page이기에 항상 0으로 고정
H_P * load_header(off_t off) {
    //Memory의 공간을 할당받아 disk에 있는 파일을 memory로 불러옴
    H_P * newhp = (H_P*)calloc(1, sizeof(H_P));
    //pread는 파일 입출력 함수 : file로부터 특정 offset에서 데이터를 읽어오는 기능 수행
    //page size가 4096이므로 이를 넘지 않도록 조절함
    if (sizeof(H_P) > pread(fd, newhp, sizeof(H_P), 0)) {

        return NULL;
    }
    //header page 불러오고 반환
    return newhp;
}


//Page를 load하는 함수 : 여기서는 특정 offset이 필요함
page * load_page(off_t off) {
    //Memory에 page를 불러오기 위해 calloc으로 메모리 공간 할당받음
    page* load = (page*)calloc(1, sizeof(page));
    //if (off % sizeof(page) != 0) printf("load fail : page offset error\n");
    //file descriptor로부터 offset부터 sizeof만큼 읽어오기
    if (sizeof(page) > pread(fd, load, sizeof(page), off)) {

        return NULL;
    }
    //새로운 페이지 load
    return load;
}

//table을 여는 함수, 정상적인 파일을 불러오게 되면 fd에 양수인 값을 리턴함
int open_table(char * pathname) {
    fd = open(pathname, O_RDWR | O_CREAT | O_EXCL | O_SYNC  , 0775);
    hp = (H_P *)calloc(1, sizeof(H_P));
    if (fd > 0) {
        //printf("New File created\n");
        hp->fpo = 0;
        hp->num_of_pages = 1;
        hp->rpo = 0;
        //일단 초기화 된 header를 써주고 
        pwrite(fd, hp, sizeof(H_P), 0);
        free(hp);
        //디스크의 header를 다시 불러옴
        hp = load_header(0);
        return 0;
    }
    fd = open(pathname, O_RDWR|O_SYNC);
    if (fd > 0) {
        //printf("Read Existed File\n");
        if (sizeof(H_P) > pread(fd, hp, sizeof(H_P), 0)) {
            return -1;
        }
        off_t r_o = hp->rpo;
        rt = load_page(r_o);
        return 0;
    }
    else return -1;
}

//disk에 특적 offset에 있는 page를 초기화
void reset(off_t off) {
    //초기화된 페이지 만들기
    page * reset;
    reset = (page*)calloc(1, sizeof(page));
    reset->parent_page_offset = 0;
    reset->is_leaf = 0;
    reset->num_of_keys = 0;
    reset->next_offset = 0;
    //초기화된 페이지를 disk에 해당 offset에 작성하기
    pwrite(fd, reset, sizeof(page), off);
    //임시로 작업했던 memory page를 free해주기
    free(reset);
    return;
}

//free page offset을 받아 그 페이지를 로드한 후 해당 페이지를 초기화하여 재사용할 수 있는 상태로 만드는 함수
void freetouse(off_t fpo) {
    //새로운 disk free page 불오기
    page * reset;
    reset = load_page(fpo);
    //Memory에서 disk free page 초기화
    reset->parent_page_offset = 0;
    reset->is_leaf = 0;
    reset->num_of_keys = 0;
    reset->next_offset = 0;
    //초기화 한 것을 다시 disk에 작성해주기
    pwrite(fd, reset, sizeof(page), fpo);
    //임시로 memory에 load 했던 page를 free 시켜주기
    free(reset);
    return;
}

//지정된 offset의 페이지를 빈 페이지 목록에 추가하고, 해당 빈 페이지로 초기화 하는 작업 하는 함수
//free page가 linked list 형태로 자리잡고 있는 것
void usetofree(off_t wbf) {
    //wbf에 있는 페이지 memory로 load
    page * utf = load_page(wbf);
    //현재 header page의 첫 번 째 free page의 offset으로 설정
    //새롭운 free page는 다음 페이지로 현재 free page중 첫 페이지를 가짐
    utf->parent_page_offset = hp->fpo;
    utf->is_leaf = 0;
    utf->num_of_keys = 0;
    utf->next_offset = 0;
    //초기화 하고 disk에 초기화 한 내용 쓰기
    pwrite(fd, utf, sizeof(page), wbf);
    //임시로 할당받았던 memory 공간 삭제
    free(utf);
    //새로 해제된 PAGE가 맨 처음 free page로 오게 함
    hp->fpo = wbf;
    //header file update 해주고
    pwrite(fd, hp, sizeof(hp), 0);
    //header file을 새롭게 load 해줌
    free(hp);
    hp = load_header(0);
    return;
}

//새로운 페이지를 할당하는 함수
off_t new_page() {
    off_t newp;
    page * np;
    off_t prev;
    //free page가 존재하면
    if (hp->fpo != 0) {
        //page offset을 첫 free page로 받아오고
        newp = hp->fpo;
        //np에 free page를 load
        np = load_page(newp);
        //첫 free page 하나가 빠졌으므로 그 다음 free page에 연결해줌
        hp->fpo = np->parent_page_offset;
        //header를 update해줌
        pwrite(fd, hp, sizeof(hp), 0);
        //header를 다시 로드 해줌
        free(hp);
        hp = load_header(0);
        free(np);
        //newp의 offset에 있는 것을 초기화하여 사용할 수 있게 함
        freetouse(newp);
        return newp;
    }
    //change previous offset to 0 is needed
    newp = lseek(fd, 0, SEEK_END);
    //if (newp % sizeof(page) != 0) printf("new page made error : file size error\n");
    reset(newp);
    hp->num_of_pages++;
    pwrite(fd, hp, sizeof(H_P), 0);
    free(hp);
    hp = load_header(0);
    return newp;
}



int cut(int length) {
    if (length % 2 == 0)
        return length / 2;
    else
        return length / 2 + 1;
}



void start_new_file(record rec) {

    page * root;
    off_t ro;
    ro = new_page();
    rt = load_page(ro);
    hp->rpo = ro;
    pwrite(fd, hp, sizeof(H_P), 0);
    free(hp);
    hp = load_header(0);
    rt->num_of_keys = 1;
    rt->is_leaf = 1;
    rt->records[0] = rec;
    pwrite(fd, rt, sizeof(page), hp->rpo);
    free(rt);
    rt = load_page(hp->rpo);
    //printf("new file is made\n");
}


char * db_find(int64_t key) {
    //root page로 부터 찾기 시작
    page * curr_page = load_page(hp->rpo);
    int i;
    char * result = NULL; 
    off_t p_offset;

    while (!curr_page->is_leaf) {
        //internal node를 돌면서 key값 비교
        for (i = 0; i < curr_page->num_of_keys; i++) {
            if (key <= curr_page->b_f[i].key) {
                break;
            }
        }
        //끝에 노드까지 도달한 경우 loop를 빠져나오면서 i가 num_of_keys가 됨
        //key는 0~num_of_keys-1까지 있겠지만 p_offset의 개수는 0~num_of_keys만큼 있을 것임
        if(i == curr_page->num_of_keys){
            //key가 꽉 차있을 경우 결국 맨 오른쪽 pointer 이용해야함 
            if(curr_page->num_of_keys == INTERNAL_MAX){
                //맨 오른쪽 pointer는 next_offset에 저장되기에 p_offset을 맨 오른쪽 pointer로 설
                p_offset = curr_page->next_offset;
            }
            //만약 key가 꽉 안 차있을 경우 num_of_keys의 pointer를 이용하면 됨
            else{
                p_offset = curr_page->b_f[i].p_offset;
            }
        }
        //이 때는 key가 딱 curr_page의 key와 동일해서 break 문을 밟고 온 것
        //이 때는 Loop에서 1이 더 증가하지 않은 상태로 나옴
        //때문에 p_offset을 그냥 i+1로 해당 key의 오른쪽 offset으로 접근하게 함
        else if(key == curr_page->b_f[i].key){
            //여기서도 i+1이 num_of_keys와 같으면 맨 오른쪽 pointer로 이동시켜줘야 함
            if(i+1 == INTERNAL_MAX){
                p_offset = curr_page->next_offset;
            }
            //꽉 차있지 않은 경우에는 그냥 현재 key의 오른쪽 pointer 이용하면 됨
            else{
                p_offset = curr_page->b_f[i+1].p_offset;
            }
        }
        //key가 search key보다 작을 때에는 그냥 왼쪽 pointer 따라가게 함
        else{
            p_offset = curr_page->b_f[i].p_offset;
        }
        // 현재 페이지를 해제하고 다음 페이지를 로드
        free(curr_page);
        curr_page = load_page(p_offset);
    }

    //이제 leaf node 왔으므로 leaf node에서 key 찾기
    for (i = 0; i < curr_page->num_of_keys; i++) {
        if (key == curr_page->records[i].key) {
            //value를 복사
            result = (char*)malloc(120 * sizeof(char));
            if (result != NULL) {
                strcpy(result, curr_page->records[i].value);
            }
            break;
        }
    }

    free(curr_page);

    //result 반환 (못 찾았으면 NULL값이 들어가 있음)
    return result;
}


int db_insert(int64_t key, char * value) {

    //현재 tree가 비어있을 경우
    if(hp->rpo == 0){
        //새로운 root생성
        page * new_root = (page*)malloc(sizeof(page));
        //새로운 페이지 할당받기
        off_t new_root_offset = new_page();
        //header page에서 루트를 가리키게 하기
        hp->rpo = new_root_offset;
        //root는 parent가 없음
        new_root->parent_page_offset = 0;
        //root의 sibling은 없으므로 next_offset을 0으로 처리
        new_root->next_offset = 0;
        //새로운 root는 root이자 leaf node
        new_root->is_leaf = 1;
        //하나의 새로운 키가 삽입되므로 num_of_keys를 1로 설정
        new_root->num_of_keys = 1;
        //next offset 값을 0으로 초기화 할까 했는데 0은 header page를 가리키므로 좀 그럼... 그래서 그냥 안 설정해둠
        //루트노드에 key값 value값 넣기 전달
        new_root->records[0].key = key;
        strcpy(new_root->records[0].value, value);
        //새로운 루트를 해당 페이지에 써주기
        pwrite(fd, new_root, sizeof(page), new_root_offset);
        //header page에도 업데이트가 일어났으니 disk write해주기
        pwrite(fd, hp, sizeof(H_P), 0);
        free(new_root);
        //이 과정에서 hp를 다시 load해줄 필요는 없는데
        //위의 함수에서 모두 이렇게 했길래 convention을 따름
        free(hp);
        hp = load_header(0);
        return 0;
    }
    page * curr_page = load_page(hp->rpo);
    int i;
    off_t p_offset = hp->rpo;

    while (!curr_page->is_leaf) {
        //internal node를 돌면서 key값 비교
        for (i = 0; i < curr_page->num_of_keys; i++) {
            if (key <= curr_page->b_f[i].key) {
                break;
            }
        }
        //끝에 노드까지 도달한 경우 loop를 빠져나오면서 i가 num_of_keys가 됨
        //key는 0~num_of_keys-1까지 있겠지만 p_offset의 개수는 0~num_of_keys만큼 있을 것임
        if(i == curr_page->num_of_keys){
            //key가 꽉 차있을 경우 결국 맨 오른쪽 pointer 이용해야함 
            if(curr_page->num_of_keys == INTERNAL_MAX){
                //맨 오른쪽 pointer는 next_offset에 저장되기에 p_offset을 맨 오른쪽 pointer로 설
                p_offset = curr_page->next_offset;
            }
            //만약 key가 꽉 안 차있을 경우 num_of_keys의 pointer를 이용하면 됨
            else{
                p_offset = curr_page->b_f[i].p_offset;
            }
        }
        //이 때는 key가 딱 curr_page의 key와 동일해서 break 문을 밟고 온 것
        //이 때는 Loop에서 1이 더 증가하지 않은 상태로 나옴
        //때문에 p_offset을 그냥 i+1로 해당 key의 오른쪽 offset으로 접근하게 함
        else if(key == curr_page->b_f[i].key){
            //여기서도 i+1이 num_of_keys와 같으면 맨 오른쪽 pointer로 이동시켜줘야 함
            if(i+1 == INTERNAL_MAX){
                p_offset = curr_page->next_offset;
            }
            //꽉 차있지 않은 경우에는 그냥 현재 key의 오른쪽 pointer 이용하면 됨
            else{
                p_offset = curr_page->b_f[i+1].p_offset;
            }
        }
        //key가 search key보다 작을 때에는 그냥 왼쪽 pointer 따라가게 함
        else{
            p_offset = curr_page->b_f[i].p_offset;
        }
        // 현재 페이지를 해제하고 다음 페이지를 로드
        free(curr_page);
        curr_page = load_page(p_offset);
    }

    for(int i = 0; i < curr_page->num_of_keys; i++){
        if(curr_page->records[i].key == key){
            printf("Key : %ld already exists\n", key);
            return -1;
        }
    }

    //이제 curr_page는 leaf node에 도달함
    //LEAF_MAX보다 작아서 아직 leaf node에 여유공간이 남아 있다면
    if(curr_page->num_of_keys < LEAF_MAX){
        free(curr_page);
        insert_in_leaf(p_offset, key, value);
        return 0;
    }

    //만약 leaf node가 꽉 차 있다면
    //Key-Rotation
    //만약 오른쪽 sibiling node가 있다면
    if(curr_page->next_offset != 0){
        //sibling leaf node를 불러오기
        page * next_page = load_page(curr_page->next_offset);
        off_t next_page_offset = curr_page->next_offset;
        //sibling에 여유공간이 있다면
        if(next_page->num_of_keys < LEAF_MAX){
            //memory에 임시 배열을 할당
            record * temp = (record*)malloc((LEAF_MAX + 1) * sizeof(record));
            int p = 0;
            //key가 curr_page record의 key보다 클때는 curr_page의 record 정보 temp에 복사
            while(p < curr_page->num_of_keys && key > curr_page->records[p].key){
                temp[p].key = curr_page->records[p].key;
                strcpy(temp[p].value, curr_page->records[p].value);
                p++;
            }
            //key가 들어갈 위치를 찾았으니 key, value값 temp에 넣어주기
            temp[p].key = key;
            strcpy(temp[p].value, value);
            //curr_page의 나머지 애들을 temp에 넣어줌
            //temp에는 key값이 들어갔으므로 p+1부터 넣어줌
            for(; p < curr_page->num_of_keys; p++){
                temp[p+1].key = curr_page->records[p].key;
                strcpy(temp[p+1].value, curr_page->records[p].value);
            }
            //이제 temp에 모든 값이 들어감
            //sibiling에 여유공간이 있으므로 temp에서 제일 큰 값을 sibling에 넣어줌
            //당연히 제일 처음에 들어갈 것임
            //먼저 한 칸씩 밀어주기
            for(int j = next_page->num_of_keys; j > 0; j--){
                next_page->records[j].key = next_page->records[j-1].key;
                strcpy(next_page->records[j].value, next_page->records[j-1].value);
            }
            //sibling 제일 처음에 insert 시 가장 큰 값 곱하기
            next_page->records[0].key = temp[LEAF_MAX].key;
            strcpy(next_page->records[0].value, temp[LEAF_MAX].value);
            next_page->num_of_keys++;

            //temp의 마지막 값을 제외한 나머지를 다시 curr_page에 돌려놓기
            for(int i = 0; i < LEAF_MAX; i++){
                curr_page->records[i].key = temp[i].key;
                strcpy(curr_page->records[i].value, temp[i].value);
            }
            //next_page에 대한 작성이 끝났으므로 더이상 이를 추가적으로 활용할 일 없으니 output함
            pwrite(fd, next_page, sizeof(page), next_page_offset);
            //curr_page에도 작성이 끝났으므로 더이상 이를 차가적으로 활용할 일 없으니 output 시킴
            pwrite(fd, curr_page, sizeof(page), p_offset);

            int64_t new_key = next_page->records[0].key;
            off_t left_child_offset = p_offset;
            //부모 노드 update
            update_parent(curr_page->parent_page_offset, left_child_offset, new_key);
            //이제 부모노드의 수정도 끝났으므로 다 free해줌 (이 함수에서는 next_page를 제외하고는 변경사항이 없으므로 free만 해주기)
            free(curr_page);
            free(next_page);
            free(temp);
            return 0;
        }
    }

    //이제 split!
    //새로운 노드 만들기
    off_t new_leaf_offset = new_page();
    //new_page()에서 파일 시스템의 최대 크기 도달하여 더이상 페이지를 할당할 수 없는 경우
    //-1을 리턴하기 때문에 이와 같이 수정해줌
    if(new_leaf_offset == -1){
        printf("No more Page\n");
        return -1;
    }
    page * new_leaf = load_page(new_leaf_offset);
    //새로운 노드는 leaf node이므로 is_leaf를 1로 설정
    new_leaf->is_leaf = 1;
    new_leaf->parent_page_offset = curr_page->parent_page_offset;
    new_leaf->num_of_keys = 0;
    //Memory에 temporary 공간 생성
    record * temp = (record*)malloc((LEAF_MAX + 1) * sizeof(record));
    int k = 0;
    //key보다 작은 것들 먼저 temp에 넣기
    while(k < curr_page->num_of_keys && key > curr_page->records[k].key){
        temp[k].key = curr_page->records[k].key;
        strcpy(temp[k].value, curr_page->records[k].value);
        k++;
    }
    //insert할 새로운 key, value 값 넣기
    temp[k].key = key;
    strcpy(temp[k].value, value);

    //나머지 record를 temp에 넣기
    for(; k < curr_page->num_of_keys; k++){
        temp[k+1].key = curr_page->records[k].key;
        strcpy(temp[k+1].value, curr_page->records[k].value);
    }

    //Memory 임시 공간에 이제 새로운 key, value를 포함해서 모두 올라감
    //sibling pointer 설정 -> 새로운 노드가 오른쪽에 형성된다고 하자
    new_leaf->next_offset = curr_page->next_offset;
    curr_page->next_offset = new_leaf_offset;

    //비교 로직중에 key와 비교하는 것이 많으므로 0으로 초기화하면 문제가 발생할 수 있기에 모든 값을 int64_t의 최대로 초기화
    for(int i = 0; i < curr_page->num_of_keys; i++){
        curr_page->records[i].key = __INT64_MAX__;
        memset(curr_page->records[i].value, '\0', sizeof(curr_page->records[i].value));
    }
    //이제 초기화했으니 들어있는 key의 개수는 없음
    curr_page->num_of_keys = 0;

    //Leaf node의 order는 31(record pointer) + 1(next_offset)이므로 32
    int middle = cut(LEAF_MAX + 1);
    //먼저 왼쪽 node의 반 넣기
    for(int i = 0; i < middle; i++){
        curr_page->records[i].key = temp[i].key;
        strcpy(curr_page->records[i].value, temp[i].value);
        curr_page->num_of_keys++;
    }

    //새로운 노드에 나머지 넣기
    for(int i = middle; i < LEAF_MAX + 1; i++){
        new_leaf->records[i-middle].key = temp[i].key;
        strcpy(new_leaf->records[i-middle].value, temp[i].value);
        new_leaf->num_of_keys++;
    }

    //부모에 대해 재귀적으로 돌기 위해 new_leaf에 최소 key값과 부모 node의 offset 가져오기
    int64_t V_key = new_leaf->records[0].key;
    int parent_offset = curr_page->parent_page_offset;

    //이제 leaf에서 작업은 끝남 output해주고 free해줌
    pwrite(fd, curr_page, sizeof(page), p_offset);
    pwrite(fd, new_leaf, sizeof(page), new_leaf_offset);
    free(curr_page);
    free(new_leaf);
    free(temp);
    //부모에 대해서 재귀적으로 돌게함
    return insert_in_parent(parent_offset, p_offset, V_key, new_leaf_offset);
}

//key_rotation에서 internal node의 key를 업데이트 하는 과정
void update_parent(off_t parent_offset, off_t left_child_offset, int64_t new_key){
    //parent page를 load
    page * parent_page = load_page(parent_offset);
    for(int i = 0; i <= parent_page->num_of_keys; i++){
        //parent page가 꽉 차 있는 경우
        if(parent_page->num_of_keys == INTERNAL_MAX){
            if(i == parent_page->num_of_keys){
                if(parent_page->next_offset == left_child_offset){
                    break;
                }
            }
            //꽉 찼지만 같은 부모인 경우
            else{
                if(parent_page->b_f[i].p_offset == left_child_offset){
                    parent_page->b_f[i].key = new_key;
                    pwrite(fd, parent_page, sizeof(page), parent_offset);
                    free(parent_page);
                    return;
                }
            }
        }
        //parent가 꽉 안 차있는 경우
        else{
            if(i == parent_page->num_of_keys){
                if(parent_page->b_f[i].p_offset == left_child_offset){
                    break;
                }
            }
            else{
                if(parent_page->b_f[i].p_offset == left_child_offset){
                    parent_page->b_f[i].key = new_key;
                    pwrite(fd, parent_page, sizeof(page), parent_offset);
                    free(parent_page);
                    return;
                }
            }
        }
    }
    //old key를 못찾았을 경우에는 parent의 parent에 old key 값이 있다는 것
    //이를 재귀적으로 타고 올라가서 고쳐줘야 함
    off_t g_parent_offset = parent_page->parent_page_offset;
    //header page가 아닐 때 까지 올라가서 계속 old key를 찾음
    if(g_parent_offset != 0){
        //한 번 왼쪽에 있던것은 계속 왼쪽 자손
        update_parent(g_parent_offset, parent_offset, new_key);
    }
    //parent_page를 free 시켜줌
    free(parent_page);
}

void insert_in_leaf(off_t L, int64_t key, char* value){
    page * leaf_node = load_page(L);
    int i;
    for(i = 0; i < leaf_node->num_of_keys; i++){
        //들어갈 곳을 찾은것
        if(key < leaf_node->records[i].key){

            break;
        }
    }
    //한 칸씩 밀어주기
    //맨 마지막에 추가하는 것을 따로 처리해줘야 하나 싶었지만 굳이 그럴 필요가 없음
    for(int j = leaf_node->num_of_keys; j > i; j--){
        leaf_node->records[j].key = leaf_node->records[j-1].key;
        strcpy(leaf_node->records[j].value, leaf_node->records[j-1].value);
    }

    //주어진 key, value 삽입
    leaf_node->records[i].key = key;
    strcpy(leaf_node->records[i].value, value);
    leaf_node->num_of_keys++;

    pwrite(fd, leaf_node, sizeof(page), L);
    free(leaf_node);
}

int insert_in_parent(off_t parent_offset, off_t left_child, int64_t key, off_t right_child){
    //당연히 left child가 원래 존재했던 node고 right child가 새로 생긴 node이기에 당연히 left child가 root인지 검사해야함
    //이때 새로운 root는 분명 leaf가 아닐것임 때문에 b_f에 값을 추가해줘야함
    if(hp->rpo == left_child){
        off_t new_node_offset = new_page();
        if(new_node_offset == -1){
            printf("No more page\n");
            return -1;
        }
        page * new_node = load_page(new_node_offset);
        page * left_child_node = load_page(left_child);
        page * right_child_node = load_page(right_child);
        //새로운 root가 internal node임을 명시
        new_node->is_leaf = 0;
        //새로운 root에 key와 pointer 값 넣어주기
        new_node->b_f[0].p_offset = left_child;
        new_node->b_f[0].key = key;
        new_node->b_f[1].p_offset = right_child;
        //header page의 root pointer 값 바꿔주기
        hp->rpo = new_node_offset;
        new_node->num_of_keys = 1;
        pwrite(fd, new_node, sizeof(page), new_node_offset);
        pwrite(fd, hp, sizeof(H_P), 0);
        free(hp);
        hp = load_header(0);

        //부모 노드 바꿔주기
        left_child_node->parent_page_offset = new_node_offset;
        right_child_node->parent_page_offset = new_node_offset;
        pwrite(fd, left_child_node, sizeof(page), left_child);
        pwrite(fd, right_child_node, sizeof(page), right_child);
        free(left_child_node);
        free(right_child_node);
        free(new_node);
        return 0;
    }

    //Internal node의 order는 leaf와 다르기 때문에 249임
    //부모에게 여유 공간이 있을 때
    //먼저 parent page를 load
    page * parent_page = load_page(parent_offset);
    if(parent_page->num_of_keys < INTERNAL_MAX){
        int i;
        //left child pointer를 찾기
        for(i = 0; i <= parent_page->num_of_keys; i++){
            if(parent_page->b_f[i].p_offset == left_child){
                break;
            }
        }
        //i번째에 지금 left child pointer가 속해있으므로 i번째 key와 i+1번째 p_offset부터 옮겨줌
        //여기서부터 next_offset 변수 때문에 두 가지 케이스로 나뉨
        //Case 1) 새로운 key와 pointer가 추가되었을 때 꽉 차는 경우
        if(parent_page->num_of_keys == INTERNAL_MAX - 1){
            //나머지 것들 이동시켜줌
            for(int j = parent_page->num_of_keys; j > i; j--){
                //next_offset으로 pointer를 이동해줘야 하는 경우
                if(j == parent_page->num_of_keys){
                    parent_page->b_f[parent_page->num_of_keys].key = parent_page->b_f[parent_page->num_of_keys - 1].key;
                    parent_page->next_offset = parent_page->b_f[parent_page->num_of_keys].p_offset;
                }
                //pointer의 index값이 이 경우 key의 index 값보다 1이 크므로 
                else{
                    parent_page->b_f[j+1].p_offset = parent_page->b_f[j].p_offset;
                    parent_page->b_f[j].key = parent_page->b_f[j-1].key;
                }
            }
        }
        //Case 2) 좀 공간이 널널하게 남아 있을 때
        else{
            for(int j = parent_page->num_of_keys; j > i; j--){
                parent_page->b_f[j+1].p_offset = parent_page->b_f[j].p_offset;
                parent_page->b_f[j].key = parent_page->b_f[j-1].key;
            }
        }

        //밀어준 후에 key, pointer 삽입
        if(parent_page->num_of_keys == INTERNAL_MAX - 1 && i == parent_page->num_of_keys){
            parent_page->b_f[i].key = key;
            parent_page->next_offset = right_child;
        }
        else{
            parent_page->b_f[i].key = key;
            parent_page->b_f[i+1].p_offset = right_child;
        }
        //parent_page key 개수 증가시키기
        parent_page->num_of_keys++;
        //parent page에 삽입이 끝났으니 output해주기

        pwrite(fd, parent_page, sizeof(page), parent_offset);
        free(parent_page);
        return 0;
    }
    //이제 부모 노드에 여유공간이 없을 때
    //부모노드에서도 split이 일어나야 함
    //split을 위한 노드를 하나 만듦
    off_t new_parent_offset = new_page();
    if(new_parent_offset == -1){
        printf("No more page\n");
        return -1;
    }
    page * new_parent = load_page(new_parent_offset);
    new_parent->parent_page_offset = parent_page->parent_page_offset;
    //새로운 노드는 internal node일 것이므로 is_leaf를 0으로 설정
    new_parent->is_leaf = 0;
    //Memory에 임시 저장할 공간을 둠 => pointer의 개수가 Internal_Max + 1개이고 추가할 pointer가 있으므로 +1 해서 INTERNAL_MAX + 2 만큼 공간 할당
    //temp의 마지막 key는 활용하지 않는 것임
    I_R * temp = (I_R*)malloc((INTERNAL_MAX + 2) * sizeof(I_R));
    int k = 0;
    //Memory 공간에 key보다 작은 것까지 복사
    while(k < parent_page->num_of_keys && key > parent_page->b_f[k].key){
        temp[k].key = parent_page->b_f[k].key;
        temp[k].p_offset = parent_page->b_f[k].p_offset;
        k++;
    }
    //여기서 궁금할 수 잇는 점은 next_offset까지 도달햇을 때 이를 복사 안하냐는 것인데
    //어차피 key때문에 위에서 모든 값이 복사되고 next_offset만 복사가 안됏다면
    //그 next_offset에 들어잇는 pointer는 left_child 일 것임
    //아무튼 일반적으로는 중간에 삽입하는 가정이고 이는 끝단에서도 잘 먹힘
    temp[k].key = key;
    temp[k].p_offset = left_child;
    temp[k+1].p_offset = right_child;

    //나머지를 memory 공간에 삽입
    for(int j = k; j < parent_page->num_of_keys; j++){
        temp[j+1].key = parent_page->b_f[j].key;
        //pointer를 next_offset을 따로 추가해줘야 하므로 분기
        if(j == INTERNAL_MAX - 1){
            temp[j+2].p_offset = parent_page->next_offset;
        }
        else{
            temp[j+2].p_offset = parent_page->b_f[j+1].p_offset;
        }
    }
    //temp에 새로운 key, pointer 값 모두가 복사됨
    //햔제 parent_page의 모든 내용 삭제
    for(int i = 0; i < parent_page->num_of_keys; i++){
        //key들을 int64max로 초기화
        parent_page->b_f[i].key = __INT64_MAX__;
        //p_offset을 0으로 초기화
        parent_page->b_f[i].p_offset = 0;
    }

    //마지막 offset을 0으로 초기화
    parent_page->next_offset = 0;
    //num_of_keys를 0으로 초기화
    parent_page->num_of_keys = 0;

    //Internal node의 ordersms 248(b_f) + 1(next_offset
    int middle = cut(INTERNAL_MAX + 1);
    //반을 왼쪽에 추가
    for(int i = 0; i < middle; i++){
        if(i != middle - 1){
            parent_page->b_f[i].key = temp[i].key;
            parent_page->num_of_keys++;
        }
        parent_page->b_f[i].p_offset = temp[i].p_offset;
    }
    //부모로 전달해줄 key를 냅두고
    int64_t new_key = temp[middle-1].key;

    //부모가 split이 일어날 때 자식 노드의 parent_page_offset을 바꿔주기 위함
    page* child_temp;
    //새로운 노드에 추가해주기
    for(int i = middle; i < INTERNAL_MAX + 2; i++){
        new_parent->b_f[i-middle].p_offset = temp[i].p_offset;
        if(i != INTERNAL_MAX + 1){
            new_parent->b_f[i-middle].key = temp[i].key;
            new_parent->num_of_keys++;
        }
        //여기서 문제가 발생 => 부모가 split이 일어나 자식 노드들의 parent_page_offset 정보의 수정이 필요해짐
        child_temp = load_page(temp[i].p_offset);
        child_temp->parent_page_offset = new_parent_offset;
        pwrite(fd, child_temp, sizeof(page), temp[i].p_offset);
        free(child_temp);
    }
    //여기까지 parent_page와 new_parent의 역할을 서로 다했기 때문에 이들을 다시 disk에 output해줌
    //먼저 gparent offset을 받아놓기
    off_t gparent_offset = parent_page->parent_page_offset;

    pwrite(fd, parent_page, sizeof(page), parent_offset);
    pwrite(fd, new_parent, sizeof(page), new_parent_offset);
    free(parent_page);
    free(new_parent);
    free(temp);
    insert_in_parent(gparent_offset, parent_offset, new_key, new_parent_offset);
}


// void print_bpt() {
//     if (hp->rpo == 0) {
//         printf("Tree is empty.\n");
//         return;
//     }

//     printf("B+ Tree keys:\n");
//     printf("hp->rpo = %ld\n", hp->rpo);
//     print_keys(hp->rpo, 0);
// }


// // B+ Tree의 키를 출력하는 함수
// void print_keys(off_t page_offset, int depth) {
//     // 노드를 로드
//     page *current_page = load_page(page_offset);

//     // 들여쓰기(Depth) 출력
//     for (int i = 0; i < depth; i++) {
//         printf("  ");
//     }

//     // 현재 노드의 키 출력
//     printf("[");
//     for (int i = 0; i < current_page->num_of_keys; i++) {
//         if (current_page->is_leaf) {
//             printf("%" PRId64, current_page->records[i].key); // 리프 노드
//         } else {
//             printf("%" PRId64, current_page->b_f[i].key); // 내부 노드
//         }
//         if (i < current_page->num_of_keys - 1) {
//             printf(", ");
//         }
//     }
//     printf("]\n");

//     // 리프 노드인 경우 탐색 종료
//     if (current_page->is_leaf) {
//         free(current_page);
//         return;
//     }

//     off_t child_offset;
//     // 내부 노드라면 자식 노드를 탐색
//     for (int i = 0; i <= current_page->num_of_keys; i++) {
//         if(current_page->num_of_keys == INTERNAL_MAX && i == current_page->num_of_keys){
//             child_offset = current_page->next_offset;
//         }
//         else{
//             child_offset = current_page->b_f[i].p_offset;
//         }
//         print_keys(child_offset, depth + 1); // 재귀적으로 자식 노드 탐색
//     }

//     free(current_page);
// }

// void print_parent(off_t child_offset){
//     page* children = load_page(child_offset);
//     page* parent = load_page(children->parent_page_offset);

//     printf("Parent page elements : [");
//     for(int i = 0; i < parent->num_of_keys; i++){
//         printf("%ld ", parent->b_f[i].key);
//     }
//     printf("]\n");
// }


int db_delete(int64_t key) {
    //Tree가 비어있는 경우
    if(hp->rpo == 0){
        printf("Tree is Empty\n");
        return -1;
    }
    page * curr_page = load_page(hp->rpo);
    int i;
    off_t p_offset = hp->rpo;

    while (!curr_page->is_leaf) {
        //internal node를 돌면서 key값 비교
        for (i = 0; i < curr_page->num_of_keys; i++) {
            if (key <= curr_page->b_f[i].key) {
                break;
            }
        }
        //끝에 노드까지 도달한 경우 loop를 빠져나오면서 i가 num_of_keys가 됨
        //key는 0~num_of_keys-1까지 있겠지만 p_offset의 개수는 0~num_of_keys만큼 있을 것임
        if(i == curr_page->num_of_keys){
            //key가 꽉 차있을 경우 결국 맨 오른쪽 pointer 이용해야함 
            if(curr_page->num_of_keys == INTERNAL_MAX){
                //맨 오른쪽 pointer는 next_offset에 저장되기에 p_offset을 맨 오른쪽 pointer로 설
                p_offset = curr_page->next_offset;
            }
            //만약 key가 꽉 안 차있을 경우 num_of_keys의 pointer를 이용하면 됨
            else{
                p_offset = curr_page->b_f[i].p_offset;
            }
        }
        //이 때는 key가 딱 curr_page의 key와 동일해서 break 문을 밟고 온 것
        //이 때는 Loop에서 1이 더 증가하지 않은 상태로 나옴
        //때문에 p_offset을 그냥 i+1로 해당 key의 오른쪽 offset으로 접근하게 함
        else if(key == curr_page->b_f[i].key){
            //여기서도 i+1이 num_of_keys와 같으면 맨 오른쪽 pointer로 이동시켜줘야 함
            if(i+1 == INTERNAL_MAX){
                p_offset = curr_page->next_offset;
            }
            //꽉 차있지 않은 경우에는 그냥 현재 key의 오른쪽 pointer 이용하면 됨
            else{
                p_offset = curr_page->b_f[i+1].p_offset;
            }
        }
        //key가 search key보다 작을 때에는 그냥 왼쪽 pointer 따라가게 함
        else{
            p_offset = curr_page->b_f[i].p_offset;
        }
        // 현재 페이지를 해제하고 다음 페이지를 로드
        free(curr_page);
        curr_page = load_page(p_offset);
    }
    //이제 curr_page가 leaf_node에 도달
    //p_offset이 이를 가리킴
    free(curr_page);
    return delete_entry(p_offset, key);
}

int delete_entry(off_t node_offset, int64_t key){
    page * node = load_page(node_offset);
    //먼저 entry를 삭제할 건데 leaf랑 internal이랑 구분해야됨
    //Leaf인 경우
    if(node->is_leaf){
        int i = 0;
        int flag = 0;
        //leaf node에서 삭제할 key 찾기
        for(; i < node->num_of_keys; i++){
            if(node->records[i].key == key){
                flag = 1;
                break;
            }
        }
        if(flag == 0){
            printf("Entry Not found\n");
            return -1;
        }

        //한 칸씩 당겨주기 (삭제시키기)
        for(; i < node->num_of_keys - 1; i++){
            node->records[i].key = node->records[i+1].key;
            strcpy(node->records[i].value, node->records[i+1].value);
        }

        //마지막 노드는 초기화
        node->records[node->num_of_keys - 1].key = __INT64_MAX__;
        memset(node->records[node->num_of_keys - 1].value, '\0', sizeof(node->records[node->num_of_keys - 1].value));

        //삭제가 완료되었으므로 key 개수 한개 줄이기
        node->num_of_keys--;
        //일단 삭제가 완료되었으므로 output
        pwrite(fd, node, sizeof(page), node_offset);
    }
    //internal node인 경우
    //주어지는 pointer는 무조건 key의 오른쪽 pointer일 것임
    //왜냐하면 delete를 하고 merge를 한 경우 
    else{
        int i = 0;
        //internal node에서 key 찾기
        for(; i < node->num_of_keys; i++){
            if(node->b_f[i].key == key){
                //만약 node가 꽉 차있고 삭제할 node와 pointer가 맨 마지막 것 일때
                if(node->num_of_keys == INTERNAL_MAX && i == node->num_of_keys - 1){
                    //마지막 key와 pointer 삭제 
                    node->b_f[i].key = __INT64_MAX__;
                    node->next_offset = 0;
                }
                //일반적으로 삭제할 key, pointer가 중간에 있을 때
                else{
                    for(int j = i; j < node->num_of_keys - 1; j++){
                        if(node->num_of_keys == INTERNAL_MAX && j == node->num_of_keys - 2){
                            node->b_f[j].key = node->b_f[j+1].key;
                            node->b_f[j+1].p_offset = node->next_offset;
                        }
                        else{
                            node->b_f[j].key = node->b_f[j+1].key;
                            node->b_f[j+1].p_offset = node->b_f[j+2].p_offset;
                        }
                    }
                }
                //삭제가 완료되었으므로 num_of_keys - 1
                node->num_of_keys--;
                break;
            }
        }
        //삭제가 완료되었으므로 output
        pwrite(fd, node, sizeof(page), node_offset);
    }

    //현재 페이지가 루트 페이지이면서 키가 하나도 없을 경우 
    if(hp->rpo == node_offset){
        if(node->num_of_keys == 0){
            //현재 노드가 leaf가 아니면서 key가 더이상 존재하지 않고 오직 하나의 child만 가지고 있을 때
            if(!node->is_leaf && node->b_f[0].p_offset != 0){
                //root page를 child로 바꿈
                hp->rpo = node->b_f[0].p_offset;
                //새로운 root page 로드
                page * child = load_page(hp->rpo);
                //새로운 root의 parent page를 header page로 설정
                child->parent_page_offset = 0;
                pwrite(fd, child, sizeof(page), hp->rpo);
                //header page의 root가 바뀌었으므로 write
                free(child);
                //usetofree 함수에 header page를 load하는 부분이 존재함 때문에 여기서 미리 output을 해줘야함
                pwrite(fd, hp, sizeof(H_P), 0);
                free(hp);
                hp = load_header(0);
                //node, 즉, 원래의 root page를 삭제해주고 Linked list에 추가해줌
                usetofree(node_offset);
            }
            //현재 node가 leaf node이고 더이상 key가 남아 있지 않을 때 
            //사실상 tree가 삭제된 케이스
            else if(node->is_leaf){
                //root page를 삭제해주고 header page의 정보를 update해줌
                hp->rpo = 0;
                //header page에 변화가 생겼으므로 output
                //usetofree함수때문에 미리 다시 load
                pwrite(fd, hp, sizeof(H_P), 0);
                free(hp);
                hp = load_header(0);
                usetofree(node_offset);
            }
            else{
                //에러가 발생함
                return -1;
            }
            free(node);
            //delete 성공했으므로 0 리턴
            return 0;
        }
        //key가 1개 이상이라면 root는 그냥 놔두어도 됨
        free(node);
        return 0;
    }

    if(node->is_leaf){
        //아직 공간이 충분하므로
        if(node->num_of_keys >= cut(LEAF_MAX)){
            free(node);
            return 0;
        }
    }
    else{
        if(node->num_of_keys >= cut(INTERNAL_MAX + 1) - 1){
            free(node);
            return 0;
        }
    }

    //이제는 진짜 여유 공간이 없음
    //Merge & Redistribution
    //한 Node에 몰아줄 수 있는 경우와 그렇지 않은 경우로 나뉨
    //여기서도 leaf인지 internal인지 경우가 나뉨
    //먼저 leaf인 경우 => parent는 무조건 internal node일 것

    //sibling node가 왼쪽일지 오른쪽일지 알려주는 variable => 왼쪽이면 1, 오른쪽이면 0
    int is_left_sibling = 0;
    off_t parent_offset = node->parent_page_offset;
    int k_prime;
    int sibling_offset = get_neighbor_offset(node_offset, parent_offset, node->num_of_keys, &is_left_sibling, &k_prime);

    //만약 여유공간이 있는 sibling을 발견했으면
    if(sibling_offset != -1){
        page * node_sibling = load_page(sibling_offset);
        //sibling node가 왼쪽 sibling이면
        if(is_left_sibling){
            //만약 node가 leaf node이면 records로 옮겨줘야 함
            if(node->is_leaf){
                //node에 있는 모든것을 왼쪽 sibling에게 넘겨주기
                for(int i = node_sibling->num_of_keys ;i < node_sibling->num_of_keys + node->num_of_keys; i++){
                    node_sibling->records[i].key = node->records[i-node_sibling->num_of_keys].key;
                    strcpy(node_sibling->records[i].value, node->records[i].value);
                }
                node_sibling->num_of_keys += node->num_of_keys;
                node->num_of_keys = 0;
                //sibling 연결해주기
                node_sibling->next_offset = node->next_offset;

                //sibling에 일단 write가 되었으므로 output 시키기
                pwrite(fd, node_sibling, sizeof(page), sibling_offset);
                usetofree(node_offset);
                free(node);
                free(node_sibling);
                return delete_entry(parent_offset, k_prime);
            }
            //sibling node가 왼쪽이고 현재 node가 internal node인 경우
            else{
                //k_prime을 sibling에 넣어주기
                node_sibling->b_f[node_sibling->num_of_keys].key = k_prime;
                //node의 첫 pointer 넘겨주기
                node_sibling->b_f[node_sibling->num_of_keys + 1].p_offset = node->b_f[0].p_offset;
                //sibling node값 증가시켜주기
                node_sibling->num_of_keys++;

                for(int i = 0; i < node->num_of_keys; i++){
                    //마지막 node와 pointer를 넘겨주는데 sibling node가 거의 꽉 차 있음
                    if(i == node->num_of_keys - 1 && node_sibling->num_of_keys == INTERNAL_MAX - 1){
                        node_sibling->b_f[node_sibling->num_of_keys].key = node->b_f[i].key;
                        node_sibling->next_offset = node->b_f[i+1].p_offset;
                    }
                    //일반적인 경우 node의 모든 pointer와 값들을 넣어줌
                    else{
                        node_sibling->b_f[node_sibling->num_of_keys].key = node->b_f[i].key;
                        node_sibling->b_f[node_sibling->num_of_keys + 1].p_offset = node->b_f[i+1].p_offset;
                    }
                    node_sibling->num_of_keys++;
                }

                //모든 node의 자식들의 parent_page_offset을 node_sibling으로 옮줌
                //어차피 node는 좀 비어있을테니 next_offset을 접근 안해도 됨
                for(int j = 0; j <= node->num_of_keys; j++){
                    page * temp = load_page(node->b_f[j].p_offset);
                    temp->parent_page_offset = sibling_offset;
                    pwrite(fd, temp, sizeof(page), node->b_f[j].p_offset);
                    free(temp);
                }

                //node_sibling으로 합치기가 끝났으므로 output 해줌
                pwrite(fd, node_sibling, sizeof(page), sibling_offset);
                //node는 삭제시켜줌
                usetofree(node_offset);
                free(node_sibling);
                free(node);
                return delete_entry(parent_offset, k_prime);
            }
        }
        //만약 sibling이 right sibling이면 right sibling을 왼쪽으로 (원래 node로 옮겨줘야 함)
        else{
            //만약 node가 leaf node면 records로 옮겨줌
            if(node->is_leaf){
                //node로 node sibling의 모든 값 옮겨줌
                for(int i = node->num_of_keys; i < node_sibling->num_of_keys + node->num_of_keys; i++){
                    node->records[i].key = node_sibling->records[i-node->num_of_keys].key;
                    strcpy(node->records[i].value, node_sibling->records[i-node->num_of_keys].value);
                }
                //node의 key 개수 증가시키기
                node->num_of_keys += node_sibling->num_of_keys;
                //node_sibling의 개수 0으로 초기화
                node_sibling->num_of_keys = 0;

                //sibling 연결해주기
                node->next_offset = node_sibling->next_offset;
                //node에 작업이 끝났으므로 output 시키기
                pwrite(fd, node, sizeof(page), node_offset);
                //node_sibling 초기화키기
                usetofree(sibling_offset);
                free(node);
                free(node_sibling);
                return delete_entry(parent_offset, k_prime);
            }
            //sibling node가 오른쪽이고 현재 node가 internal node인 경우
            else{
                //node에 k_prime값 넣어주기
                node->b_f[node->num_of_keys].key = k_prime;
                //node_sibling의 맨 왼쪽 포인터값 넣어주기
                node->b_f[node->num_of_keys+1].p_offset = node_sibling->b_f[0].p_offset;
                //node의 key 개수 증가시키기
                node->num_of_keys++;

                for(int i = 0; i < node_sibling->num_of_keys; i++){
                    //마지막 node와 pointer를 넘겨주는데 node가 거의 꽉 찬 상태 인 경우
                    if(i == node_sibling->num_of_keys - 1 && node->num_of_keys == INTERNAL_MAX - 1){
                        node->b_f[node->num_of_keys].key = node_sibling->b_f[i].key;
                        node->next_offset = node_sibling->b_f[i+1].p_offset;
                    }
                    //일반적인 경우 node_sibling의 모든 pointer와 값들을 넣어줌
                    else{
                        node->b_f[node->num_of_keys].key = node_sibling->b_f[i].key;
                        node->b_f[node->num_of_keys + 1].p_offset = node_sibling->b_f[i+1].p_offset;
                    }
                    node->num_of_keys++;
                }

                //node_sibling의 모든 children의 parent_page_offset을 node로 옮겨줌
                //어차피 node_sibling 역시 꽉 차 있지 않으므로 next_offset 접근 할 필요 없음
                for(int j = 0; j <= node_sibling->num_of_keys; j++){
                    page * temp = load_page(node_sibling->b_f[j].p_offset);
                    temp->parent_page_offset = node_offset;
                    pwrite(fd, temp, sizeof(page), node_sibling->b_f[j].p_offset);
                    free(temp);
                }

                //node로 합쳐지기가 끝났으므로 output 해줌
                pwrite(fd, node, sizeof(page), node_offset);
                //node_sibling 삭제시키기
                usetofree(sibling_offset);
                free(node_sibling);
                free(node);
                return delete_entry(parent_offset, k_prime);
            }
        }
    }

    //이제 redistribution
    off_t sibling_node_offset = get_neighbor_for_redistribution(node_offset, parent_offset, &is_left_sibling, &k_prime);

    if(sibling_node_offset != -1){
        page * sibling_node = load_page(sibling_node_offset);

        //왼쪽 sibling에서 가져오기
        if(is_left_sibling){
            //현재 node가 leaf node인 경우
            if(node->is_leaf){
                //일단 node를 밀어주기
                for(int i = node->num_of_keys; i > 0; i--){
                    node->records[i].key = node->records[i-1].key;
                    strcpy(node->records[i].value, node->records[i-1].value);
                }
                //왼쪽 sibling node의 마지막 key와 pointer를 node에 삽입
                node->records[0].key = sibling_node->records[sibling_node->num_of_keys - 1].key;
                strcpy(node->records[0].value, sibling_node->records[sibling_node->num_of_keys - 1].value);
                page * parent = load_page(parent_offset);
                //부모의 k_prime을 왼쪽 sibling의 마지막 key값으로 교체
                for(int i = 0; i < parent->num_of_keys; i++){
                    if(parent->b_f[i].key == k_prime){
                        parent->b_f[i].key = sibling_node->records[sibling_node->num_of_keys - 1].key;
                    }
                }

                //각가의 node의 num_of_keys 값 조정해주기
                node->num_of_keys++;
                sibling_node->num_of_keys--;
                pwrite(fd, node, sizeof(page), node_offset);
                pwrite(fd, sibling_node, sizeof(page), sibling_node_offset);
                pwrite(fd, parent, sizeof(page), parent_offset);
                free(node);
                free(sibling_node);
                free(parent);
                return 0;
            }
            //Left sibling에서 가져오고 Internal node인 경우
            else{
                //일단 node 밀어주기
                for(int i = node->num_of_keys; i > 0; i--){
                    //key는 일반적으로 그냥 밀어주면 됨
                    node->b_f[i].key = node->b_f[i-1].key;
                    //pointer의 경우 next_offset을 고려해서 밀어줌
                    //(key, pointer) 이렇게 한 쌍을 오른쪾으로 옮겨준다고 생각하면됨
                    if(node->num_of_keys == INTERNAL_MAX - 1 && i == node->num_of_keys){
                        node->next_offset = node->b_f[i+1].p_offset;
                    }
                    else{
                        node->b_f[i+2].p_offset = node->b_f[i+1].p_offset;
                    }
                }
                //아직 맨 처음 포인터가 안옮겨졌을 것임
                //맨 처음 포인터도 밀어주기
                node->b_f[1].p_offset = node->b_f[0].p_offset;
                //밀어주기 끝

                //node에 값을 가져옴 K_PRIME이랑 sibling_node의 마지막 pointer
                //이 값을 node의 첫 pointer와 key에 삽입
                //sibling node가 꽉 차 있는 경우에는 마지막 pointer가 next_offset이므로
                if(sibling_node->num_of_keys == INTERNAL_MAX){
                    node->b_f[0].p_offset = sibling_node->next_offset;
                    page * child = load_page(node->b_f[0].p_offset);
                    child->parent_page_offset = node_offset;
                    pwrite(fd, child, sizeof(page), node->b_f[0].p_offset);
                    free(child);
                }
                else{
                    node->b_f[0].p_offset = sibling_node->b_f[sibling_node->num_of_keys].p_offset;
                    page * child = load_page(node->b_f[0].p_offset);
                    child->parent_page_offset = node_offset;
                    pwrite(fd, child, sizeof(page), node->b_f[0].p_offset);
                    free(child);
                }
                node->b_f[0].key = k_prime;

                page * parent = load_page(parent_offset);
                
                //parent node의 key값 수정
                for(int i = 0; i < parent->num_of_keys; i++){
                    if(parent->b_f[i].key == k_prime){
                        parent->b_f[i].key = sibling_node->b_f[sibling_node->num_of_keys - 1].key;
                    }
                }


                //key 개수 update
                node->num_of_keys++;
                sibling_node->num_of_keys--;

                pwrite(fd, node, sizeof(page), node_offset);
                pwrite(fd, sibling_node, sizeof(page), sibling_node_offset);
                pwrite(fd, parent, sizeof(page), parent_offset);
                free(node);
                free(parent);
                free(sibling_node);
                return 0;
            }
        }
        //오른쪽 sibling에 대해서 redistribution 수행하는 경우
        else{
            //현재 node가 leaf node인 경우
            if(node->is_leaf){
                //오른쪽 sibling의 첫 번째 node와 pointer를 node에 가져옴
                //사실상 오른쪽 sibling의 첫 번째 key가 k_prime일 것임
                node->records[node->num_of_keys].key = sibling_node->records[0].key;
                strcpy(node->records[node->num_of_keys].value, sibling_node->records[0].value);

                //sibling node의 첫 번째 key, value를 node에 넘겨주었으므로 한 칸씩 당겨주기
                for(int i = 0; i < sibling_node->num_of_keys - 1; i++){
                    sibling_node->records[i].key = sibling_node->records[i+1].key;
                    strcpy(sibling_node->records[i].value, sibling_node->records[i+1].value);
                }

                //부모의 k_prime을 교체해줄 것임
                int new_k_prime = sibling_node->records[0].key;
                page * parent = load_page(parent_offset);

                //부모의 k_prime값 업데이트
                for(int i = 0; i < parent->num_of_keys; i++){
                    if(parent->b_f[i].key == k_prime){
                        parent->b_f[i].key = new_k_prime;
                        break;
                    }
                }

                //num_of_keys 조정해줌
                node->num_of_keys++;
                sibling_node->num_of_keys--;

                pwrite(fd, node, sizeof(page), node_offset);
                pwrite(fd, sibling_node, sizeof(page), sibling_node_offset);
                pwrite(fd, parent, sizeof(page), parent_offset);
                free(node);
                free(sibling_node);
                free(parent);
                return 0;
            }
            //오른쪽 형제에 대해 redistribution을 수행하고 node가 internal node인 경우
            else{
                //부모노드의 k_prime과 오른쪽 형제의 맨 왼쪽 pointer를 node의 맨 마지막에 추가
                if(node->num_of_keys != INTERNAL_MAX - 1){
                    node->b_f[node->num_of_keys].key = k_prime;
                    node->b_f[node->num_of_keys + 1].p_offset = sibling_node->b_f[0].p_offset;
                    node->num_of_keys++;
                    page * child = load_page(node->b_f[node->num_of_keys + 1].p_offset);
                    child->parent_page_offset = node_offset;
                    pwrite(fd, child, sizeof(page), node->b_f[node->num_of_keys + 1].p_offset);
                    free(child);
                }
                else{
                    node->b_f[node->num_of_keys].key = k_prime;
                    node->next_offset = sibling_node->b_f[0].p_offset;
                    node->num_of_keys++;
                    page * child = load_page(node->next_offset);
                    child->parent_page_offset = node_offset;
                    pwrite(fd, child, sizeof(page), node->next_offset);
                    free(child);
                }
                //옮겨진 pointer의 parent page offset 바꾸기
                page * child = load_page(sibling_node->b_f[0].p_offset);
                child->parent_page_offset = node_offset;
                pwrite(fd, child, sizeof(page), sibling_node->b_f[0].p_offset);

                //새로운 k_prime
                int new_k_prime = sibling_node->b_f[0].key;

                //오른쪽 node에서 당겨주기
                for(int i = 0; i < sibling_node->num_of_keys - 1; i++){
                    sibling_node->b_f[i].key = sibling_node->b_f[i+1].key;
                    sibling_node->b_f[i].p_offset = sibling_node->b_f[i+1].p_offset;
                }
                //만약 꽉 차 있다면 마지막 pointer가 next_offset일 임
                if(sibling_node->num_of_keys == INTERNAL_MAX){
                    sibling_node->b_f[sibling_node->num_of_keys - 1].p_offset = sibling_node->next_offset;
                }
                else{
                    sibling_node->b_f[sibling_node->num_of_keys - 1].p_offset = sibling_node->b_f[sibling_node->num_of_keys].p_offset;
                }

                //key 개수 변화시키기
                sibling_node->num_of_keys--;

                //parent page에 새로운 k_prime 값으로 교체
                page * parent = load_page(parent_offset);
                for(int i = 0; i < parent->num_of_keys; i++){
                    if(parent->b_f[i].key == k_prime){
                        parent->b_f[i].key = new_k_prime;
                    }
                }

                pwrite(fd, node, sizeof(page), node_offset);
                pwrite(fd, sibling_node, sizeof(page), sibling_node_offset);
                pwrite(fd, parent, sizeof(page), parent_offset);
                free(parent);
                free(node);
                free(sibling_node);
                return 0;
            }
        }
    }
}


off_t get_neighbor_offset (off_t node_offset, off_t parent_offset, int node_num_of_keys, int* is_left_sibling, int* k_prime){
    int i;
    off_t sibling_offset;
    page * parent = load_page(parent_offset);

    for(i = 0; i <= parent->num_of_keys; i++){
        //Case 1) 현재 노드가 마지막 offset에 위치할 경우 (parent node가 꽉 차고)
        if(parent->num_of_keys == INTERNAL_MAX && i == parent->num_of_keys){
            if(parent->next_offset == node_offset){
                //left sibling을 load
                page * temp = load_page(parent->b_f[i-1].p_offset);
                //현재 sibling을 찾는 node가 leaf인 경우
                if(temp->is_leaf){
                    //왼쪽 node에 공간이 충분할 때
                    if(temp->num_of_keys + node_num_of_keys <= LEAF_MAX){
                        //left sibling을 리턴
                        *is_left_sibling = 1;
                        *k_prime = parent->b_f[i-1].key;
                        sibling_offset = parent->b_f[i-1].p_offset;
                        free(temp);
                        free(parent);
                        return sibling_offset;
                    }
                    //못 찾았을 경우에는 오른쪽 sibling이 존재하지 않는 것이므로 -1 턴
                    else{
                        free(temp);
                        free(parent);
                        return -1;
                    }
                }
                //현재 sibling을 찾는 node가 internal node인 경우
                else{
                    //왼쪽 sibling에 여유공간이 있을 때
                    //k_prime까지 생각해야 하므로 + 1이 INTERNAL_MAX를 초과하지 않는지 검사해야함
                    if(temp->num_of_keys + node_num_of_keys + 1 <= INTERNAL_MAX){
                        *is_left_sibling = 1;
                        *k_prime = parent->b_f[i-1].key;
                        sibling_offset = parent->b_f[i-1].p_offset;
                        free(temp);
                        free(parent);
                        return sibling_offset;
                    }
                    //못 찾았을 때에는 오른쪽 sibling이 없으므로 -1 리턴
                    else{
                        free(temp);
                        free(parent);
                        return -1;
                    }
                }
            }
        }
        //Case 2) node가 parent의 첫번째 p_offset에 위치해 right_sibling 밖에 없을 때
        else if(i == 0){
            if(parent->b_f[i].p_offset == node_offset){
                //오른쪽 sibling load
                page * temp = load_page(parent->b_f[i+1].p_offset);

                //sibling가 leaf인 경우
                if(temp->is_leaf){
                    if(temp->num_of_keys + node_num_of_keys <= LEAF_MAX){
                        *is_left_sibling = 0;
                        *k_prime = parent->b_f[i].key;
                        sibling_offset = parent->b_f[i+1].p_offset;
                        free(temp);
                        free(parent);
                        return sibling_offset;
                    }
                    //오른쪽 sibling이 여유롭지 않은 경우는 -1 return
                    else{
                        free(temp);
                        free(parent);
                        return -1;
                    }
                }
                //node가 internal_node인 경우
                else{
                    //여유 공간이 있으면 오른쪽 sibling 리턴
                    if(temp->num_of_keys + node_num_of_keys + 1 <= INTERNAL_MAX){
                        *is_left_sibling = 0;
                        *k_prime = parent->b_f[i].key;
                        sibling_offset = parent->b_f[i+1].p_offset;
                        free(temp);
                        free(parent);
                        return sibling_offset;
                    }
                    //오른쪽 sibling이 여유롭지 않으면 -1 리턴
                    else{
                        free(temp);
                        free(parent);
                        return -1;
                    }
                }
            }
        }
        //지금까지는 애초에 오른쪽 node가 없었던 경우임
        //이제는 일반적인 경우
        //parent에서 node_offset을 찾으면
        else if(parent->b_f[i].p_offset == node_offset){
            //먼저 왼쪽부터 확인
            //근데 node가 leftmost pointer라면 오른쪽을 봐야함
            page * temp = load_page(parent->b_f[i-1].p_offset);
            //현재 node가 leaf 인 경우
            if(temp->is_leaf){
                //왼쪽 node에 여유공간이 있는 경우
                if(temp->num_of_keys + node_num_of_keys <= LEAF_MAX){
                    *is_left_sibling = 1;
                    *k_prime = parent->b_f[i-1].key;
                    sibling_offset = parent->b_f[i-1].p_offset;
                    free(temp);
                    free(parent);
                    //왼쪽 sibling의 index 리턴
                    return sibling_offset;
                }
                else{
                    //만약 왼쪽 node에는 더이상 여유공간이 없는 경우 오른쪽 node 확인
                    free(temp);
                    //parent가 꽉 차있고 node_offset이 마지막 딱 직전에 위치할 때 (오른쪽 sibling offset은 next_offset)
                    if(parent->num_of_keys == INTERNAL_MAX && i == parent->num_of_keys - 1){
                        temp = load_page(parent->next_offset);
                        //오른쪽 노드의 여유 공간이 있는 것
                        if(temp->num_of_keys + node_num_of_keys <= LEAF_MAX){
                            //오른쪽 node offset 리턴
                            *is_left_sibling = 0;
                            *k_prime = parent->b_f[i].key;
                            sibling_offset = parent->next_offset;
                            free(temp);
                            free(parent);
                            return sibling_offset;
                        }
                        else{
                            //이제 오른쪽, 왼쪽 둘 다 여유가 없는 상태이미로 redistribution이 필요한 상태
                            free(temp);
                            free(parent);
                            return -1;
                        }
                    }
                    //parent가 꽉 안 차있는 경우
                    else{
                        temp = load_page(parent->b_f[i+1].p_offset);
                        //오른쪽 sibling에 여유공간이 있을때
                        if(temp->num_of_keys + node_num_of_keys <= LEAF_MAX){
                            *is_left_sibling = 0;
                            *k_prime = parent->b_f[i].key;
                            sibling_offset = parent->b_f[i+1].p_offset;
                            free(temp);
                            free(parent);
                            return sibling_offset;
                        }
                        //이제 더이상 오른쪽, 왼쪽 sibling 모두 안되는 것이므로 -1 리턴
                        else{
                            free(temp);
                            free(parent);
                            return -1;
                        }
                    }
                }
            }
            //현재 node가 internal node인 경우
            else{
                //왼쪽 것에 여유 공간이 남아 있다면
                if(temp->num_of_keys + node_num_of_keys + 1 <= INTERNAL_MAX){
                    *k_prime = parent->b_f[i-1].key;
                    *is_left_sibling = 1;
                    sibling_offset = parent->b_f[i-1].p_offset;
                    free(temp);
                    free(parent);
                    return sibling_offset;
                }
                //왼쪽이 여유롭지 않으므로 오른쪽 뒤져야 함
                else{
                    free(temp);
                    //parent가 꽉 차 있고 node가 마지막 직전에 위치한 경우
                    if(parent->num_of_keys == INTERNAL_MAX && i == parent->num_of_keys - 1){
                        temp = load_page(parent->next_offset);
                        //오른쪽 node의 여유공간이 있으면 
                        if(temp->num_of_keys + node_num_of_keys + 1 <= INTERNAL_MAX){
                            *k_prime = parent->b_f[i].key;
                            *is_left_sibling = 0;
                            sibling_offset = parent->next_offset;
                            free(temp);
                            free(parent);
                            return sibling_offset;
                        }
                        //오른쪽 node에도 여유공간이 없으면 
                        else{
                            free(temp);
                            free(parent);
                            return -1;
                        }
                    }
                    //parent가 꽉 차 있지 않은 경우
                    else{
                        //오른쪽 sibling load
                        temp = load_page(parent->b_f[i+1].p_offset);
                        //오른쪽 sibling에 여유공간이 있으면
                        if(temp->num_of_keys + node_num_of_keys + 1 <= INTERNAL_MAX){
                            *k_prime = parent->b_f[i].key;
                            *is_left_sibling = 0;
                            sibling_offset = parent->b_f[i+1].p_offset;
                            free(temp);
                            free(parent);
                            //오른쪽 sibling의 index 리턴
                            return sibling_offset;
                        }
                        //이제는 양쪽 모두 여유 공간이 없는 것이므로 -1 return
                        else{
                            free(temp);
                            free(parent);
                            return -1;
                        }
                    }
                }
            }
        }
    }
}

//redistribution은 일반적으로 왼쪽으로 일어남
//하지만 만약 현재 node가 부모의 맨 왼쪽 sibling이라면 오른쪽 sibling으로 redistribution 수행
off_t get_neighbor_for_redistribution(off_t node_offset, off_t parent_offset, int* is_left_sibling, int* k_prime){
    off_t sibling_offset;
    page * parent = load_page(parent_offset);
    //일반적으로 왼쪽 sibling으로 redistribution 수행
    if(node_offset != parent->b_f[0].p_offset){
        int i = 0;
        *is_left_sibling = 1;
        for(; i < parent->num_of_keys; i++){
            if(parent->b_f[i].p_offset == node_offset){
                break;
            }
        }
        *k_prime = parent->b_f[i-1].key;
        sibling_offset = parent->b_f[i-1].p_offset;
        free(parent);
        return sibling_offset;
    }
    //node가 첫 번째 p_offset인 경우
    *is_left_sibling = 0;
    *k_prime = parent->b_f[0].key;
    sibling_offset = parent->b_f[1].p_offset;
    free(parent);
    return sibling_offset;
}

// void print_leaf(){
//     page * curr_page = load_page(hp->rpo);
//     while(!curr_page->is_leaf){
//         off_t next = curr_page->b_f[0].p_offset;
//         free(curr_page);
//         curr_page = load_page(next);
//     }

//     while(curr_page->next_offset){
//         for(int i = 0; i < curr_page->num_of_keys; i++){
//             printf("(%ld, %s), ", curr_page->records[i].key, curr_page->records[i].value);
//         }
//         off_t next = curr_page->next_offset;
//         free(curr_page);
//         curr_page = load_page(next);
//     }
//     free(curr_page);
// }
//fin