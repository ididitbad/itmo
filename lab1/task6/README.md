```

# add new disk in VirtualBox and boot system

gpart create -s GPT ada1
gpart add -t freebsd-ufs -a 1M ada1

newfs -U /dev/ada1p1

mkdir /mnt/ufs
mount /dev/ada1p1 /mnt/ufs
echo '/dev/ada1p1 /mnt/ufs ufs rw 2 2' >> /etc/fstab

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

ls

```
