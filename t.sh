while true
do
	sleep 0.01
	echo -n '["hello","3"]'
	RANDOM=`od -An -N2 -i /dev/random`
	echo -n '["TS.ADD", "randomArduino", "*", "'$RANDOM'","LABELS","property","random"]'
	DATE=`date`
	echo -n '["publish","dst/node/system/loopback","'$DATE'"]'
	echo -n '["psubscribe","dst/node/*"]'
done

