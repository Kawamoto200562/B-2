#include <stdio.h>
#include <sqlite3.h>
#include <register.h>
#include <update.h>
#include <string.h>

int main() {
    int select;
    printf("1: 新規登録\n 2: 更新\n ");
    (void)scanf("%d", &select);

    if (select == 1) {
        register_infomation();
    }
    else if (select == 2) {
        update_infomation();
    }
    else {
        printf("やり直し\n");
    }

    return 0;
}

