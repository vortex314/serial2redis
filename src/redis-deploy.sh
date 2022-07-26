sudo cp redis-server.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart redis-server
sudo systemctl status redis-server
sudo systemctl enable redis-server
