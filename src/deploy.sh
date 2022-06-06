sudo cp udp2redis.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart udp2redis
sudo systemctl status udp2redis