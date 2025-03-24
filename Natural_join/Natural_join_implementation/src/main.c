#include "bpt.h"

int main(){
    int64_t input;
    char instruction;
    int table_id;
    char buf[120];
    char *result;

    // Assignment 4
    open_table("table1.db", 0);
    open_table("table2.db", 1);

    while(scanf("%c", &instruction) != EOF){
        switch(instruction){
            case 'i':
                scanf("%d", &table_id);
                scanf("%ld %s", &input, buf);
                db_insert(input, buf, table_id);
                break;
            case 'f':
                scanf("%d", &table_id);
                scanf("%ld", &input);
                result = db_find(input, table_id);
                if (result) {
                    printf("Key: %ld, Value: %s\n", input, result);
                    free(result);
                }
                else
                    printf("Not Exists\n");

                fflush(stdout);
                break;
            case 'd':
                scanf("%d", &table_id);
                scanf("%ld", &input);
                db_delete(input, table_id);
                break;
            
            // Assignment 4
            case 'j':
                db_join();
                break;


            //////////////////////////////////////////////////////////////////

            case 'q':
                while (getchar() != (int)'\n');
                return EXIT_SUCCESS;
                break;   

        }
        while (getchar() != (int)'\n');
    }
    printf("\n");
    return 0;
}



