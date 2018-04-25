
if [ "$#" -ne 3 ]; then
	echo "usage: $0 program start_value end_value"
	exit 1	
fi

delta=$(( ($3-$2)/10 ))

echo "N1 = $2"
echo "N2 = $3"

for (( i = $2; i <= $3; i += delta ))
do
	eval "./$1 $i"
done

exit 0
