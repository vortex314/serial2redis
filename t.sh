while true
do
	sleep 3
	echo -n '["hello","3"]'
	sleep 3
	DATE=`date`
	echo -n '["publish","dst/node/system/loopback","$DATE"]'
	sleep 3
	echo -n '["psubscribe","dst/node/*"]'
done

