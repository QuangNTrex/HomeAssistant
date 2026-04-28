Dưới đây là quy trình chuẩn, đã loại bỏ toàn bộ lỗi, để bạn có thể dựng lại từ đầu một cách ổn định.

I. Chuẩn bị hệ thống
sudo apt update && sudo apt upgrade -y
sudo apt install docker.io docker-compose-plugin -y
sudo systemctl enable docker
sudo usermod -aG docker $USER

→ đăng nhập lại

II. Mount HDD
sudo mkfs.ext4 /dev/sdb1
sudo mkdir -p /mnt/hdd
sudo mount /dev/sdb1 /mnt/hdd

Lấy UUID:

sudo blkid /dev/sdb1

Thêm vào /etc/fstab:

UUID=xxxx /mnt/hdd ext4 defaults,noatime,nodiratime 0 2

Phân quyền:

sudo chown -R 1000:1000 /mnt/hdd
III. Tạo cấu trúc dữ liệu
mkdir -p /mnt/hdd/seafile-data/mysql
mkdir -p /mnt/hdd/seafile-data/seafile
cd /mnt/hdd/seafile-data
IV. docker-compose (chuẩn)
services:
  db:
    image: mariadb:10.11
    container_name: seafile-mysql
    restart: always
    environment:
      - MYSQL_ROOT_PASSWORD=yourpassword
      - MYSQL_LOG_CONSOLE=true
    volumes:
      - /mnt/hdd/seafile-data/mysql:/var/lib/mysql

  memcached:
    image: memcached:1.6
    container_name: seafile-memcached
    restart: always

  seafile:
    image: seafileltd/seafile-mc:latest
    container_name: seafile
    restart: always
    ports:
      - "8080:80"
    volumes:
      - /mnt/hdd/seafile-data/seafile:/shared
    environment:
      - DB_HOST=db
      - DB_ROOT_PASSWD=yourpassword
      - TIME_ZONE=Asia/Ho_Chi_Minh
      - SEAFILE_ADMIN_EMAIL=your@email.com
      - SEAFILE_ADMIN_PASSWORD=yourpassword
      - SEAFILE_SERVER_LETSENCRYPT=false
      - SEAFILE_SERVER_HOSTNAME=seafile.quangnt.id.vn
    depends_on:
      - db
      - memcached

  thumbnail:
    image: seafileltd/thumbnail-server:13.0-latest
    container_name: seafile-thumbnail
    restart: always
    volumes:
      - /mnt/hdd/seafile-data/seafile:/opt/seafile-data
    environment:
      - SEAFILE_MYSQL_DB_HOST=db
      - SEAFILE_MYSQL_DB_USER=root
      - SEAFILE_MYSQL_DB_PASSWORD=yourpassword
      - SEAFILE_VOLUME=/opt/seafile-data
      - TIME_ZONE=Asia/Ho_Chi_Minh
      - INNER_SEAHUB_SERVICE_URL=http://seafile:80
      - SEAF_SERVER_STORAGE_TYPE=disk
      - JWT_PRIVATE_KEY=YOUR_KEY
V. Chạy hệ thống
docker compose up -d

Kiểm tra:

docker ps
VI. Cấu hình Seafile (sau khi chạy lần đầu)

File đúng:

nano /mnt/hdd/seafile-data/seafile/seafile/conf/seahub_settings.py

Thêm:

ENABLE_VIDEO_THUMBNAIL = True

JWT_PRIVATE_KEY = "YOUR_KEY"

CSRF_TRUSTED_ORIGINS = ["https://seafile.quangnt.id.vn"]

SERVICE_URL = "https://seafile.quangnt.id.vn"
FILE_SERVER_ROOT = "https://seafile.quangnt.id.vn/seafhttp"

SECURE_PROXY_SSL_HEADER = ('HTTP_X_FORWARDED_PROTO', 'https')

Restart:

docker compose restart
VII. Cấu hình nginx (thumbnail)
nano /mnt/hdd/seafile-data/seafile/nginx/seafile.nginx.conf

Thêm:

location /thumbnail/ {
    proxy_pass http://seafile-thumbnail:80;
}

Restart:

docker compose restart
VIII. Cloudflare Tunnel
1. Login + tạo tunnel
cloudflared tunnel login
cloudflared tunnel create seafile-tunnel
2. Tạo config
mkdir -p ~/.cloudflared
nano ~/.cloudflared/config.yml
tunnel: TUNNEL_ID
credentials-file: /home/pi/.cloudflared/TUNNEL_ID.json

ingress:
  - hostname: seafile.quangnt.id.vn
    service: http://localhost:8080
  - service: http_status:404
3. Route DNS

(đảm bảo đã xóa record cũ trong Cloudflare)

cloudflared tunnel route dns seafile-tunnel seafile.quangnt.id.vn
4. Cài service
sudo mkdir -p /etc/cloudflared
sudo cp ~/.cloudflared/* /etc/cloudflared/

sudo cloudflared service install
sudo systemctl start cloudflared
sudo systemctl enable cloudflared
IX. Kiểm tra
Local
curl http://localhost:8080
Public
https://seafile.quangnt.id.vn
X. Kiến trúc hoàn chỉnh
Browser
   ↓
Cloudflare DNS
   ↓
Cloudflare Tunnel
   ↓
Raspberry Pi 5
   ↓
Docker:
   ├─ Seafile
   ├─ MariaDB
   ├─ Memcached
   └─ Thumbnail Server
   ↓
HDD (/mnt/hdd)
XI. Trạng thái cuối

Hệ thống đúng khi:

truy cập domain không lỗi 1033
login không lỗi CSRF
upload/download hoạt động
preview ảnh/video có thumbnail
Kết luận

Quy trình đúng gồm 4 phần cốt lõi:

HDD mount + permission đúng
Docker compose cấu hình DB thống nhất
Seafile config đúng domain + proxy
Cloudflare Tunnel chạy dạng service

Nếu cần mở rộng tiếp, bước hợp lý tiếp theo là:

backup tự động (rclone / rsync)
giới hạn CPU thumbnail
tối ưu upload file lớn qua Cloudflare