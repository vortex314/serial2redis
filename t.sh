while true
do
	sleep 1
	echo '["HELLO",3]' 
	sleep 1
	DATE=`date`
	echo '["PUBLISH","X","'$DATE'"]' 
	echo '["PSUBSCRIBE","*"]'
done

