sudo cp USB0redis.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart USB0redis
sudo systemctl status USB0redis
sudo systemctl enable USB0redis
