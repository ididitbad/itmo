```

## test

cd /mnt/ufs

echo '
#include <stdio.h>

int main(int argc, char **argv)
{
        if (argc != 3) {
                printf("usage: %s from to\n", argv[0]);
                return (1);
        }
        int error = rename(argv[1], argv[2]);
        if (error) {
                perror(NULL);
                return (error);
        }
        return (0);
}
' > rename.c

clang rename.c -o rename

touch file
./rename file /file

touch file
./rename file file/

touch file
./rename file /f/i/l/e

```
