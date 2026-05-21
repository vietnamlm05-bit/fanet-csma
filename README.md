# Hướng Dẫn Sử Dụng FANET CSMA/CA Simulation

Repo này đã chứa sẵn toàn bộ source code ns-3.39 cùng file mô phỏng `scratch/fanet-csma-simulation.cc`. Bạn chỉ cần clone về, build và chạy.

---

## 1. Mục tiêu kịch bản

Tìm **"điểm gãy" (Breaking Point)** của giao thức CSMA/CA khi:
- Bị tước cơ chế bảo vệ RTS/CTS
- Trong môi trường di động (FANET — Flying Ad-hoc Network)
- Mật độ thiết bị tăng dần theo thời gian

Mạng giả lập gồm **1 Gateway tĩnh** ở giữa bản đồ 500×500 m và **tối đa 29 Drone** cứ 5 phút lại có 5 drone "cất cánh" thêm, phát UDP về Gateway. Tổng tải lý thuyết tại mốc 1800 s là ~11.87 Mbps — vượt ngưỡng băng thông thực tế của 802.11g.

| Thông số | Giá trị mặc định |
|---|---|
| WiFi standard | 802.11g Ad-hoc |
| Data rate / Control rate | 12 Mbps / 6 Mbps (cố định) |
| Packet size / rate | 512 B / 100 pps/nút |
| Path loss exponent | 3.0 (NLOS vùng thiên tai) |
| RTS/CTS threshold mặc định | 2346 (tắt cho gói 512 B) |
| Mobility | RandomWaypoint 5–15 m/s, pause 2 s |
| Thời gian mô phỏng | 1800 s (30 phút) |
| Wave staggered | 5 drone / 300 s |

---

## 2. Build

Vào thư mục ns-3.39 rồi build:

```bash
cd ns-3.39
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

> Lần build đầu mất ~10–30 phút tùy CPU. Sau đó chỉ rebuild file scratch nên rất nhanh (<1 phút).

Nếu muốn chạy nhanh hơn cho các kịch bản lớn:

```bash
./ns3 configure --build-profile=optimized
./ns3 build
```

---

## 3. Chạy nhanh — 4 kịch bản chính

```bash
cd ns-3.39

# 1) Di động, RTS/CTS TẮT  (kịch bản chính — phơi bày hidden node)
./ns3 run "scratch/fanet-csma-simulation --mobile=true --rtsCtsThreshold=2346"

# 2) Di động, RTS/CTS BẬT  (đối chứng — chứng minh RTS/CTS cứu hiệu năng)
./ns3 run "scratch/fanet-csma-simulation --mobile=true --rtsCtsThreshold=0"

# 3) Tĩnh, RTS/CTS TẮT     (Lab 5 baseline)
./ns3 run "scratch/fanet-csma-simulation --mobile=false --rtsCtsThreshold=2346"

# 4) Tĩnh, RTS/CTS BẬT
./ns3 run "scratch/fanet-csma-simulation --mobile=false --rtsCtsThreshold=0"
```

Sau mỗi lần chạy, kết quả sẽ:
- In ra console (xem mục [5](#5-đọc-output-console))
- Append vào `results/csv/fanet-csma-file1-results.csv`
- Append milestone vào `results/csv/fanet-csma-file1-milestones.csv`
- Ghi FlowMonitor chi tiết vào `results/flowmon/fanet-csma-n{N}-{mode}.flowmon.xml`

> **Lưu ý:** CSV luôn **append**, không ghi đè. Nếu muốn file riêng cho mỗi lần chạy, dùng `--csvFile=results/csv/<tên_riêng>.csv`.

---

## 4. Các tham số CLI thường dùng

Xem danh sách đầy đủ:
```bash
./ns3 run "scratch/fanet-csma-simulation --PrintHelp"
```

| Tham số | Mặc định | Ý nghĩa |
|---|---|---|
| `--nNodes` | 30 | Tổng node (1 gateway + n-1 drone), 2–30 |
| `--mobile` | true | `true`=FANET, `false`=tĩnh |
| `--rtsCtsThreshold` | 2346 | 2346=RTS/CTS tắt với gói 512 B; 0=bật cho mọi gói |
| `--simTime` | 1800 | Thời gian mô phỏng (s) |
| `--dronesPerWave` | 5 | Số drone mỗi đợt |
| `--launchInterval` | 300 | Khoảng cách giữa các đợt (s) |
| `--packetSize` | 512 | Kích thước UDP payload (bytes) |
| `--packetRate` | 100 | Số gói/s/nút |
| `--minSpeed` / `--maxSpeed` | 5 / 15 | Tốc độ drone (m/s) |
| `--pauseTime` | 2.0 | Thời gian nghỉ tại waypoint (s) |
| `--areaSide` | 500 | Cạnh bản đồ vuông (m) |
| `--rngRun` | 1 | Số RNG run (lặp lại kết quả) |
| `--enablePcap` | false | Ghi pcap vào `results/pcap/` |
| `--writeNetAnim` | false | Ghi animation vào `results/netanim/` |
| `--csvFile` | `results/csv/fanet-csma-file1-results.csv` | Đường dẫn CSV summary |

### Ví dụ truyền tham số

```bash
# Test nhanh với 10 drone, 300 s
./ns3 run "scratch/fanet-csma-simulation --nNodes=10 --simTime=300"

# Tắt staggered, dùng all-at-once để so sánh
./ns3 run "scratch/fanet-csma-simulation --staggeredLaunch=false"

# Chỉ định file CSV riêng để tránh trộn với run trước
./ns3 run "scratch/fanet-csma-simulation --csvFile=results/csv/test1.csv --milestoneCsvFile=results/csv/test1-ms.csv"
```

---

## 5. Đọc output console

```
============================================================
 FANET CSMA/CA WIFI AD-HOC - LAB 5 RESULT
============================================================

-- Scenario --
  Mode                    : mobile (FANET RandomWaypoint)
  Nodes                   : 30 total = 1 gateway + 29 clients
  Map size                : 500 x 500 m

-- WiFi and CSMA/CA --
  RTS/CTS threshold       : 2346 bytes
  RTS/CTS status          : disabled for payload
  Path loss exponent      : 3.00

-- Traffic --
  Packet rate per client  : 100.00 pkt/s
  Active clients at stop  : 29
  Max offered load        : 11.87 Mbps

-- Performance --
  Throughput              : 0.847 Mbps          ← thông lượng thực
  PDR                     : 7.15 %              ← tỷ lệ gói thành công
  Packet loss             : 92.85 %
  Average delay           : 1234.56 ms
  Gateway range exit events : 4821              ← lần drone bay ra khỏi tầm
  Link tearing rate       : 2.678 events/s

-- Staggered Launch Milestones --
  300s  /  5 active clients : PDR 94.32%, Thr 0.512 Mbps, Delay 8.23 ms
  600s  / 10 active clients : PDR 87.65%, Thr 1.021 Mbps, Delay 45.12 ms
  900s  / 15 active clients : PDR 62.18%, Thr 1.485 Mbps, Delay 189.34 ms
  1200s / 20 active clients : PDR 35.42%, Thr 1.702 Mbps, Delay 567.89 ms
  1500s / 25 active clients : PDR 18.76%, Thr 1.801 Mbps, Delay 1234.56 ms
  1800s / 29 active clients : PDR 12.45%, Thr 1.823 Mbps, Delay 2156.78 ms
```

**Phần Milestone là quan trọng nhất** — dùng để vẽ đường cong PDR/Throughput/Delay theo mật độ thiết bị.

---

## 6. Phân tích kết quả từ CSV

### Cấu trúc CSV milestone

Mỗi mốc 300s sẽ có 1 dòng với các cột:
- `activeClients` — số drone đang phát
- `pdrPercent`, `throughputMbps`, `avgDelayMs` — 3 chỉ số chính
- `offeredLoadMbps` — tải lý thuyết
- `gatewayRangeExitEvents`, `linkTearingRatePerSecond` — chỉ có ý nghĩa khi `mobile=true`

### Vẽ đồ thị bằng Python

```python
import pandas as pd
import matplotlib.pyplot as plt

# Đọc 2 lần chạy (RTS off vs on) với cùng nNodes=30, mobile=true
df = pd.read_csv("results/csv/fanet-csma-file1-milestones.csv")

# Tách 2 kịch bản theo rtsCtsThreshold
df_off = df[df["rtsCtsThreshold"] == 2346]
df_on  = df[df["rtsCtsThreshold"] == 0]

# Vẽ PDR theo số drone
plt.figure(figsize=(10, 6))
plt.plot(df_off["activeClients"], df_off["pdrPercent"], "r-o", label="RTS/CTS OFF")
plt.plot(df_on["activeClients"],  df_on["pdrPercent"],  "g-s", label="RTS/CTS ON")
plt.xlabel("Số drone hoạt động")
plt.ylabel("Packet Delivery Rate (%)")
plt.title("Ảnh hưởng RTS/CTS lên PDR — FANET Mobile")
plt.legend(); plt.grid(True)
plt.savefig("results/plots/pdr-comparison.png", dpi=150)
plt.show()
```

> Cài thư viện một lần: `pip install pandas matplotlib`

---

## 7. Chạy nhiều RNG run để lấy trung bình

Một lần chạy chỉ là 1 mẫu — để đảm bảo độ tin cậy thống kê nên chạy 5–10 lần với `--rngRun` khác nhau:

```bash
for run in 1 2 3 4 5; do
  ./ns3 run "scratch/fanet-csma-simulation \
    --mobile=true --rtsCtsThreshold=2346 --rngRun=$run \
    --csvFile=results/csv/sweep-rts-off-run${run}.csv \
    --milestoneCsvFile=results/csv/sweep-rts-off-run${run}-ms.csv"
done
```

Sau đó tính trung bình ± độ lệch chuẩn:

```python
import pandas as pd, glob
files = glob.glob("results/csv/sweep-rts-off-run*-ms.csv")
df = pd.concat([pd.read_csv(f) for f in files])
g = df.groupby("activeClients")[["pdrPercent","throughputMbps","avgDelayMs"]].agg(["mean","std"])
print(g)
```

---

## 8. Visualize bằng NetAnim

NetAnim đã đi kèm trong `ns-allinone-3.39/netanim-3.109/`.

```bash
# 1) Bật ghi animation
cd ns-3.39
./ns3 run "scratch/fanet-csma-simulation --mobile=true --writeNetAnim=true --netAnimMobilityPollInterval=0.2"

# 2) Mở animation (đường dẫn netanim có thể khác chút tùy phiên bản)
cd ../netanim-3.109
./NetAnim
```

Trong UI:
1. **File → Open XML Trace File** → chọn `ns-3.39/results/netanim/fanet-csma-n30-mobile.netanim.xml`
2. Click **Play** để xem drone di chuyển và packet truyền
3. **Gateway** màu xanh dương, **Drone** màu cam

Tip:
- Giảm `--netAnimMobilityPollInterval` xuống `0.1` để animation mượt hơn (file lớn hơn)
- Giảm `--netAnimMaxPackets` nếu file quá nặng

---

## 9. Capture traffic bằng PCAP

```bash
./ns3 run "scratch/fanet-csma-simulation --enablePcap=true --pcapPrefix=results/pcap/run1"
```

Sẽ tạo `results/pcap/run1-0-0.pcap`, `run1-0-1.pcap`, … (một file/node).

Mở bằng Wireshark:
```bash
wireshark results/pcap/run1-0-0.pcap &
```

Filter hữu ích:
- `udp.port == 9` — chỉ traffic ứng dụng
- `wlan.fc.type_subtype == 0x1b` — RTS/CTS frames (kiểm tra RTS/CTS có bật không)

---

## 10. Output files — Tổng quan

Tất cả output đều ghi vào `ns-3.39/results/`:

```
ns-3.39/results/
├── csv/
│   ├── fanet-csma-file1-results.csv       ← summary mỗi lần chạy (1 dòng/run)
│   └── fanet-csma-file1-milestones.csv    ← milestone 300s/600s/.../1800s
├── flowmon/
│   └── fanet-csma-n{N}-{mode}.flowmon.xml ← chi tiết từng flow
├── pcap/                                  ← chỉ khi --enablePcap=true
├── netanim/                               ← chỉ khi --writeNetAnim=true
└── logs/                                  ← bạn tự tạo nếu muốn redirect console
```

---

## 11. Xử lý lỗi thường gặp

### Lỗi "Command not found: ./ns3"
Bạn chưa `cd` vào thư mục `ns-3.39`. Cũng đừng nhầm với `./waf` của các phiên bản cũ — ns-3.39 dùng `./ns3`.

### Build lỗi
```bash
./ns3 clean
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

### "fatal error: ns3/netanim-module.h: No such file or directory"
Module NetAnim chưa được bật. Configure lại:
```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
```
Nếu vẫn lỗi, kiểm tra Qt5 dev đã cài chưa (ns-3.39 cần Qt5 để build NetAnim).

### CSV bị trộn dữ liệu nhiều lần chạy
Đây là tính năng (append). Để tách:
```bash
./ns3 run "scratch/fanet-csma-simulation --csvFile=results/csv/myrun.csv --milestoneCsvFile=results/csv/myrun-ms.csv"
```

### Mô phỏng chạy quá chậm
1. Build optimized: `./ns3 configure --build-profile=optimized && ./ns3 build`
2. Giảm `--simTime` để test trước
3. Tắt `--writeNetAnim` và `--enablePcap` (mặc định đã tắt)

### Kết quả khác nhau giữa các lần chạy
Bình thường — đó là tính ngẫu nhiên. Để tái lập kết quả, đặt cùng `--rngRun=N` cho mỗi lần.

---

## 12. Cheat sheet — Quy trình làm báo cáo

```bash
cd ns-3.39

# 1) Build optimized một lần
./ns3 configure --build-profile=optimized --enable-examples --enable-tests
./ns3 build

# 2) Chạy 5 RNG run cho 4 kịch bản (20 lần chạy ~ 1-2h tùy máy)
for run in 1 2 3 4 5; do
  for mobile in true false; do
    for rts in 2346 0; do
      label="${mobile}-rts${rts}-run${run}"
      ./ns3 run "scratch/fanet-csma-simulation \
        --mobile=${mobile} --rtsCtsThreshold=${rts} --rngRun=${run} \
        --csvFile=results/csv/${label}.csv \
        --milestoneCsvFile=results/csv/${label}-ms.csv"
    done
  done
done

# 3) Mở Python/Jupyter, đọc CSV, vẽ đồ thị PDR/Throughput/Delay theo activeClients
```

Xong là có đủ số liệu cho **2 đường đồ thị** (RTS off vs on) × **2 mode** (mobile vs static) × **mean ± std**.
