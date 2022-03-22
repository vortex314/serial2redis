while true
do
	sleep 1
	echo -n '["hello","3"]'
	sleep 1
	DATE=`date`
	echo -n '["publish","dst/node/system/loopback","$DATE"]'
	sleep 1
	echo -n '["psubscribe","dst/node/*"]'
done

