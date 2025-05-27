#include <stdio.h>
#include <sqlite3.h>
#include <register.h>
#include <windows.h>

    int main() {
        SetConsoleOutputCP(65001);
        int select;

        while (1) {
            printf("該当の番号を半角で入力してください\n");
            printf("1:登録 2:変更 3:削除 4:参照\n");
            scanf_s("%d", &select);

            if (select == 1) {
                register_select_menu();
            }
            else if (select == 2) {
                Update();
            }
            else if (select == 3) {
                printf("delete\n");
                break;
            }
            else if (select == 4) {
                printf("reference\n");
                break;
            }
            else {
                printf("対象外の番号が選択されました。もう一度入力してください\n");

            }
        }

        return 0;
    }

