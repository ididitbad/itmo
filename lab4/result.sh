
cd bin

for i in *; do
	../func_n.sh $i 122446 22800000 > ../$i.txt
done

cd -
