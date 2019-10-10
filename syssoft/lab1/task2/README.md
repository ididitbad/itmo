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
> file
chmod 0700 file
> file
chmod 1700 file
> file

```
