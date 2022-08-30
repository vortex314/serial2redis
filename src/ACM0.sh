sudo cp ACM0.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl restart ACM0
sudo systemctl status ACM0
sudo systemctl enable ACM0
