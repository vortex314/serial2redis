while true
do
	echo -n '["hello","3"]'
	DATE=`date`
	echo -n '["publish","dst/node/system/loopback","$DATE"]'
	echo -n '["psubscribe","dst/node/*"]'
done

