sudo cp udp2redis.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart udp2redis
sudo systemctl status udp2redis
sudo systemctl enable udp2redis

sudo cp ws2redis.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart ws2redis
sudo systemctl status ws2redis
sudo systemctl enable ws2redis