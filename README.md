# Hướng Dẫn Sử Dụng FANET CSMA/CA Simulation

Repo này đã chứa sẵn toàn bộ source code ns-3.39 cùng file mô phỏng `scratch/fanet-csma-simulation.cc`. Bạn chỉ cần clone về, build và chạy.


## 1. Mục tiêu kịch bản

Tìm **"điểm gãy" (Breaking Point)** của giao thức CSMA/CA khi:
- Bị tước cơ chế bảo vệ RTS/CTS
- Trong môi trường di động (FANET — Flying Ad-hoc Network)
- Mật độ thiết bị tăng dần theo thời gian

Mạng giả lập gồm **1 Gateway tĩnh** ở giữa bản đồ 500×500 m và **tối đa 29 Drone** cứ 5 phút lại có 5 drone "cất cánh" thêm, phát UDP về Gateway. Tổng tải lý thuyết tại mốc 1800 s là ~11.87 Mbps — vượt ngưỡng băng thông thực tế của 802.11g.

| Thông số | Giá trị mặc định |
|---|---|
| WiFi standard | 802.11g Ad-hoc |# Hướng Dẫn Sử Dụng FANET CSMA/CA Compact Simulation


```text
scratch/fanet-csma-simulation.cc
```

Bản compact này được rút gọn từ bản GitHub `vietnamlm05-bit/fanet-csma`, nhưng vẫn giữ phần lõi của bài Project Netsim:

- Gateway tĩnh.
- Drone di động bằng `RandomWaypointMobilityModel`.
- WiFi 802.11g ad-hoc.
- CSMA/CA.
- Bật/tắt RTS/CTS bằng `rtsCtsThreshold`.
- UDP client/server.
- FlowMonitor để đo throughput, PDR, packet loss, delay.
- Tự tạo CSV summary và FlowMonitor XML.

Các phần đã bỏ để code gọn hơn:

- Không NetAnim.
- Không PCAP.
- Không milestone CSV.
- Không `writeNetAnim`, `enablePcap`, `writeMilestoneCsv`, `milestoneCsvFile`.

Nếu dùng README cũ trên GitHub, các phần NetAnim/PCAP/milestone CSV sẽ không còn đúng với bản compact này.

---

## 1. Cấu Trúc Kịch Bản

Mạng mô phỏng gồm:

- `Node 0`: Gateway, chạy UDP server, đứng yên ở trung tâm bản đồ.
- `Node 1..N-1`: Drone, chạy UDP client, gửi dữ liệu về Gateway.
- Topology logic: nhiều drone gửi trực tiếp về một Gateway.
- Topology wireless: WiFi ad-hoc, không có Access Point.
- Mobility chính: drone di chuyển bằng `RandomWaypointMobilityModel`.

Mục tiêu:

- Đánh giá hiệu năng CSMA/CA trong mạng FANET.
- So sánh mobile và static baseline.
- So sánh RTS/CTS tắt và bật.
- Thu các chỉ số: throughput, PDR, packet loss, delay, gateway range exit events, link tearing rate.

---

## 2. Tham Số Mặc Định

| Tham số | Mặc định | Ý nghĩa |
|---|---:|---|
| `nNodes` | `30` | Tổng node, gồm 1 Gateway + 29 drone |
| `mobile` | `true` | `true` = FANET di động, `false` = static baseline |
| `staggeredLaunch` | `true` | Drone bắt đầu gửi theo từng wave |
| `dronesPerWave` | `5` | Mỗi wave kích hoạt 5 drone gửi traffic |
| `launchInterval` | `300` | Khoảng cách giữa hai wave, đơn vị giây |
| `packetSize` | `512` | UDP payload, đơn vị byte |
| `packetRate` | `100` | Số packet/giây/client |
| `simTime` | `1800` | Thời gian mô phỏng, đơn vị giây |
| `serverStart` | `1.0` | Gateway UDP server start |
| `clientStart` | `2.0` | Wave client đầu tiên start |
| `areaSide` | `500` | Bản đồ vuông 500m x 500m |
| `minSpeed` / `maxSpeed` | `5 / 15` | Vận tốc drone, m/s |
| `pauseTime` | `2.0` | Thời gian drone dừng tại waypoint |
| `pathLossExponent` | `3.0` | Hệ số suy hao LogDistance |
| `dataMode` | `ErpOfdmRate12Mbps` | Tốc độ data frame |
| `controlMode` | `ErpOfdmRate6Mbps` | Tốc độ control frame |
| `rtsCtsThreshold` | `2346` | Tắt RTS/CTS với packet 512 byte |
| `writeCsv` | `true` | Ghi CSV summary |
| `writeFlowXml` | `true` | Ghi FlowMonitor XML |

Offered load tối đa với 29 drone:

```text
29 * 512 bytes * 8 * 100 pkt/s = 11.8784 Mbps
```

### 2.1. Cách Lấy Số Liệu Đầu Vào

Trong bài này, "số liệu đầu vào" không phải là một dataset có sẵn từ bên ngoài. Số liệu đầu vào chính là bộ tham số dùng để cấu hình mỗi lần chạy mô phỏng.

Có 4 nơi để lấy số liệu đầu vào:

| Nguồn | Dùng để làm gì |
|---|---|
| Bảng tham số mặc định ở mục 2 | Ghi phần cấu hình mặc định trong báo cáo |
| Lệnh chạy `./ns3 run ...` | Biết mỗi kịch bản đang đổi tham số nào |
| Console output | Kiểm tra lại mode, số node, RTS/CTS, packet size, simTime, offered load |
| CSV summary | Lưu input chính và output metric trên cùng một dòng để vẽ biểu đồ |

Lệnh xem toàn bộ input mà chương trình hỗ trợ:

```bash
./ns3 run "fanet-csma-simulation --PrintHelp"
```

Ví dụ chạy một cấu hình và lưu input/output vào CSV riêng:

```bash
./ns3 run "fanet-csma-simulation \
  --nNodes=30 \
  --mobile=true \
  --rtsCtsThreshold=2346 \
  --packetSize=512 \
  --packetRate=100 \
  --simTime=1800 \
  --csvFile=results/csv/input-output-n30-mobile-rts-off.csv \
  --flowXmlFile=results/flowmon/input-output-n30-mobile-rts-off.flowmon.xml"
```

Sau khi chạy, xem dòng dữ liệu:

```bash
head -n 1 results/csv/input-output-n30-mobile-rts-off.csv
tail -n 1 results/csv/input-output-n30-mobile-rts-off.csv
```

Các cột input chính đã được ghi trong CSV:

| Cột CSV | Loại số liệu | Ý nghĩa |
|---|---|---|
| `mode` | input | `mobile` hoặc `static` |
| `nNodes` | input | Tổng số node |
| `clients` | input suy ra | Số drone gửi, bằng `nNodes - 1` |
| `rtsCtsThreshold` | input | `2346` là RTS/CTS tắt, `0` là bật |
| `packetSize` | input | Kích thước UDP payload |
| `packetRate` | input | Packet/giây/client |
| `simTime` | input | Thời gian mô phỏng |
| `serverStart` | input | Thời điểm server start |
| `clientStart` | input | Thời điểm client đầu tiên start |
| `offeredLoadMbps` | input suy ra | Tải lý thuyết đưa vào mạng |

Các cột output metric trong CSV:

| Cột CSV | Loại số liệu | Ý nghĩa |
|---|---|---|
| `measuredFlows` | output | Số flow UDP được FlowMonitor đo |
| `txPackets` | output | Tổng số packet đã gửi |
| `rxPackets` | output | Tổng số packet Gateway nhận thành công |
| `rxBytes` | output | Byte nhận theo FlowMonitor |
| `rxPayloadBytes` | output | Payload byte nhận thành công |
| `lostPackets` | output | Packet mất |
| `pdrPercent` | output | Packet Delivery Ratio |
| `lossRatePercent` | output | Tỷ lệ mất gói |
| `throughputMbps` | output | Throughput thực nhận |
| `avgDelayMs` | output | Delay trung bình |
| `gatewayRangeExitEvents` | output | Số lần drone rời vùng Gateway |
| `linkTearingRatePerSecond` | output | Tốc độ đứt liên kết |
| `outOfRangeSamplePercent` | output | Phần trăm sample ngoài vùng Gateway |

Một số input cố định không được ghi vào CSV summary để giữ file gọn, nhưng vẫn phải ghi trong báo cáo:

| Input cố định | Giá trị mặc định | Lấy ở đâu |
|---|---:|---|
| `areaSide` | `500` | Console `[Scenario] Map size` hoặc bảng mục 2 |
| `minSpeed/maxSpeed` | `5/15` | Bảng mục 2 hoặc `--PrintHelp` |
| `pauseTime` | `2.0` | Bảng mục 2 hoặc `--PrintHelp` |
| `pathLossExponent` | `3.0` | Console `[WiFi and CSMA/CA]` |
| `dataMode` | `ErpOfdmRate12Mbps` | Console `[WiFi and CSMA/CA]` |
| `controlMode` | `ErpOfdmRate6Mbps` | Console `[WiFi and CSMA/CA]` |
| `dronesPerWave` | `5` | Console `[Traffic]` |
| `launchInterval` | `300` | Console `[Traffic]` |

Khi viết báo cáo, nên chia bảng dữ liệu thành 2 phần:

1. Bảng input configuration: ghi `nNodes`, `mobile/static`, `RTS/CTS`, `packetSize`, `packetRate`, `simTime`, `areaSide`, `speed`, `pathLossExponent`.
2. Bảng output metrics: lấy từ CSV các cột `throughputMbps`, `pdrPercent`, `lossRatePercent`, `avgDelayMs`, `linkTearingRatePerSecond`.

Ví dụ bảng input cho 4 kịch bản chính:

| Scenario | `mobile` | `rtsCtsThreshold` | RTS/CTS | Node sweep | Packet | Rate | Time |
|---|---:|---:|---|---|---:|---:|---:|
| Mobile RTS off | `true` | `2346` | Tắt | `2,5,10,15,20,25,30` | 512 B | 100 pkt/s | 1800 s |
| Mobile RTS on | `true` | `0` | Bật | `2,5,10,15,20,25,30` | 512 B | 100 pkt/s | 1800 s |
| Static RTS off | `false` | `2346` | Tắt | `2,5,10,15,20,25,30` | 512 B | 100 pkt/s | 1800 s |
| Static RTS on | `false` | `0` | Bật | `2,5,10,15,20,25,30` | 512 B | 100 pkt/s | 1800 s |

---

## 3. Build

Vào thư mục gốc ns-3.39:

```bash
cd ns-3.39
./ns3 configure
./ns3 build
```

Nếu muốn chạy nhanh hơn:

```bash
./ns3 configure --build-profile=optimized
./ns3 build
```

Trên máy của bạn nếu folder có tên khác, chỉ cần `cd` vào đúng thư mục chứa file `ns3`.

Ví dụ local:

```bash
cd "/home/siuu/Desktop/ns 3-39(1)"
./ns3 build
```

---

## 4. Chạy Nhanh

Chạy kịch bản mặc định, mobile FANET, RTS/CTS tắt:

```bash
./ns3 run fanet-csma-simulation
```

Tương đương:

```bash
./ns3 run "fanet-csma-simulation --mobile=true --rtsCtsThreshold=2346"
```

Chạy test nhanh 10 giây, không ghi file:

```bash
./ns3 run "fanet-csma-simulation --simTime=10 --writeCsv=false --writeFlowXml=false"
```

Chạy smoke test nhỏ để kiểm tra UDP có nhận được gói:

```bash
./ns3 run "fanet-csma-simulation --nNodes=3 --simTime=10 --areaSide=50"
```

Nếu smoke test nhỏ có PDR cao nhưng kịch bản 500m có PDR thấp hoặc 0, đó thường là do khoảng cách/path loss/mobility, không nhất thiết là lỗi code.

---

## 5. Bốn Kịch Bản Chính

### 5.1. Mobile, RTS/CTS tắt

```bash
./ns3 run "fanet-csma-simulation --mobile=true --rtsCtsThreshold=2346"
```

Đây là kịch bản chính để thấy CSMA/CA khi không có RTS/CTS bảo vệ.

### 5.2. Mobile, RTS/CTS bật

```bash
./ns3 run "fanet-csma-simulation --mobile=true --rtsCtsThreshold=0"
```

Dùng để so sánh RTS/CTS có cải thiện PDR/delay khi có hidden node hay không.

### 5.3. Static baseline, RTS/CTS tắt

```bash
./ns3 run "fanet-csma-simulation --mobile=false --rtsCtsThreshold=2346"
```

Dùng làm baseline tĩnh để so với mạng drone di động.

### 5.4. Static baseline, RTS/CTS bật

```bash
./ns3 run "fanet-csma-simulation --mobile=false --rtsCtsThreshold=0"
```

Dùng để tách ảnh hưởng của RTS/CTS khỏi ảnh hưởng của mobility.

---

## 6. Output Console

Output chính có dạng:

```text
============================================================
 FANET CSMA/CA WIFI AD-HOC - LAB 5 RESULT
============================================================

[Scenario]
  Mode                         : mobile (FANET RandomWaypoint)
  Nodes                        : 30 total = 1 gateway + 29 clients
  Gateway                      : Node 0, UDP server
  Client nodes                 : Node 1..29, UDP clients
  Routing                      : one-hop UDP to gateway, no relay routing
  Map size                     : 500 x 500 m
  Gateway position             : (250.0, 250.0, 0.0)
  Mobility start               : t=0 for all drones

[WiFi and CSMA/CA]
  WiFi standard                : 802.11g ad-hoc
  Data mode                    : ErpOfdmRate12Mbps
  Control mode                 : ErpOfdmRate6Mbps
  RTS/CTS threshold            : 2346 bytes
  RTS/CTS status               : disabled for payload
  Path loss exponent           : 3.00

[Traffic]
  Packet size                  : 512 bytes
  Packet rate per client       : 100.00 pkt/s
  Simulation time              : 1800.00 s
  Server start                 : 1.00 s
  First client start           : 2.00 s
  Launch model                 : staggered launch
  Launch scope                 : traffic + range accounting
  Drones per wave              : 5
  Launch interval              : 300.00 s
  Active clients at stop       : 29
  Max offered load             : 11.878400 Mbps

[Performance]
  Throughput                   : ... Mbps
  Throughput basis             : application payload bytes / simTime
  PDR                          : ... %
  Packet loss                  : ... %
  Average delay                : ... ms
  Gateway range exit events    : ...
  Link tearing rate            : ... events/s
  Link tearing basis           : gateway exits / simTime
  Out-of-range samples         : ... %

[Raw counters]
  Measured flows               : ...
  Tx packets                   : ...
  Rx packets                   : ...
  Rx bytes                     : ...
  Rx payload bytes             : ...
  Lost packets                 : ...

[Output files]
  CSV summary                  : results/csv/fanet-csma-compact-results.csv
  FlowMonitor XML              : results/flowmon/fanet-csma-n30-mobile.flowmon.xml
  Milestone CSV                : disabled
  PCAP prefix                  : disabled
  NetAnim XML                  : disabled
============================================================
```

Ý nghĩa nhanh:

- `Tx packets`: số packet UDP đã gửi.
- `Rx packets`: số packet UDP Gateway nhận thành công.
- `PDR`: tỷ lệ nhận thành công.
- `Throughput`: thông lượng payload thực nhận.
- `Average delay`: delay trung bình của packet nhận thành công.
- `Gateway range exit events`: số lần drone từ trong vùng Gateway chuyển ra ngoài vùng.
- `Link tearing rate`: số lần exit trung bình mỗi giây.

Lưu ý: code này không gửi ICMP ping thật. Nếu nói "số gói ping thành công" theo nghĩa chung, hãy dùng `Rx packets` và `PDR` của UDP traffic.

---

## 7. File Kết Quả

Mặc định chương trình tự tạo thư mục trong `results/`.

### 7.1. CSV summary

Mặc định:

```text
results/csv/fanet-csma-compact-results.csv
```

CSV này append thêm 1 dòng sau mỗi lần chạy.

Các cột chính:

```text
mode,nNodes,clients,rtsCtsThreshold,packetSize,packetRate,simTime,
serverStart,clientStart,offeredLoadMbps,measuredFlows,
txPackets,rxPackets,rxBytes,rxPayloadBytes,lostPackets,
pdrPercent,lossRatePercent,throughputMbps,avgDelayMs,
gatewayRangeExitEvents,linkTearingRatePerSecond,outOfRangeSamplePercent
```

Nếu muốn file riêng:

```bash
./ns3 run "fanet-csma-simulation --csvFile=results/csv/mobile-rts-off.csv"
```

### 7.2. FlowMonitor XML

Mặc định:

```text
results/flowmon/fanet-csma-n{N}-{mode}.flowmon.xml
```

Ví dụ:

```text
results/flowmon/fanet-csma-n30-mobile.flowmon.xml
results/flowmon/fanet-csma-n30-static.flowmon.xml
```

Quan trọng: tên XML mặc định không có `rtsCtsThreshold`, nên nếu chạy cùng `nNodes` và cùng `mode` nhưng đổi RTS/CTS, XML có thể bị ghi đè.

Khi so sánh RTS/CTS, nên đặt `flowXmlFile` riêng:

```bash
./ns3 run "fanet-csma-simulation --mobile=true --rtsCtsThreshold=2346 --flowXmlFile=results/flowmon/n30-mobile-rts-off.flowmon.xml"
./ns3 run "fanet-csma-simulation --mobile=true --rtsCtsThreshold=0    --flowXmlFile=results/flowmon/n30-mobile-rts-on.flowmon.xml"
```

---

## 8. Các Tham Số CLI Hỗ Trợ

Xem toàn bộ option:

```bash
./ns3 run "fanet-csma-simulation --PrintHelp"
```

Các option quan trọng:

| Option | Ý nghĩa |
|---|---|
| `--nNodes=30` | Tổng số node, hợp lệ 2..30 |
| `--mobile=true` | Bật mô hình FANET di động |
| `--mobile=false` | Chạy static baseline |
| `--staggeredLaunch=true` | Bật gửi theo wave |
| `--rtsCtsThreshold=2346` | RTS/CTS tắt với packet 512 byte |
| `--rtsCtsThreshold=0` | RTS/CTS bật |
| `--packetSize=512` | Kích thước UDP payload |
| `--packetRate=100` | Packet/giây/client |
| `--simTime=1800` | Thời gian mô phỏng |
| `--serverStart=1` | Thời điểm server start |
| `--clientStart=2` | Thời điểm client đầu tiên start |
| `--dronesPerWave=5` | Số drone mỗi wave |
| `--launchInterval=300` | Khoảng cách giữa các wave |
| `--areaSide=500` | Kích thước vùng bay |
| `--minSpeed=5` | Tốc độ nhỏ nhất của drone |
| `--maxSpeed=15` | Tốc độ lớn nhất của drone |
| `--pauseTime=2` | Pause time tại waypoint |
| `--communicationRange=140` | Range dùng để tính gateway exit |
| `--rangeSampleInterval=0.5` | Chu kỳ lấy mẫu range |
| `--pathLossExponent=3.0` | Hệ số suy hao LogDistance |
| `--dataMode=ErpOfdmRate12Mbps` | Data mode WiFi |
| `--controlMode=ErpOfdmRate6Mbps` | Control mode WiFi |
| `--rngRun=1` | Run number để tái lập random |
| `--writeCsv=true` | Ghi CSV summary |
| `--csvFile=...` | Đường dẫn CSV |
| `--writeFlowXml=true` | Ghi FlowMonitor XML |
| `--flowXmlFile=...` | Đường dẫn XML |

Các option KHÔNG hỗ trợ trong bản compact:

```text
--enablePcap
--pcapPrefix
--writeNetAnim
--netAnimFile
--netAnimMobilityPollInterval
--netAnimMaxPackets
--writeMilestoneCsv
--milestoneCsvFile
```

Nếu truyền các option này, chương trình sẽ báo lỗi vì bản compact không định nghĩa chúng.

---

## 9. Chạy Sweep Theo Node

### 9.1. Sweep 7 mốc theo Project Netsim rút gọn

Chạy các node:

```text
2, 5, 10, 15, 20, 25, 30
```

Mobile, RTS/CTS tắt:

```bash
for n in 2 5 10 15 20 25 30; do
  ./ns3 run "fanet-csma-simulation \
    --nNodes=${n} \
    --mobile=true \
    --rtsCtsThreshold=2346 \
    --csvFile=results/csv/sweep-mobile-rts-off.csv \
    --flowXmlFile=results/flowmon/n${n}-mobile-rts-off.flowmon.xml"
done
```

Mobile, RTS/CTS bật:

```bash
for n in 2 5 10 15 20 25 30; do
  ./ns3 run "fanet-csma-simulation \
    --nNodes=${n} \
    --mobile=true \
    --rtsCtsThreshold=0 \
    --csvFile=results/csv/sweep-mobile-rts-on.csv \
    --flowXmlFile=results/flowmon/n${n}-mobile-rts-on.flowmon.xml"
done
```

Static, RTS/CTS tắt:

```bash
for n in 2 5 10 15 20 25 30; do
  ./ns3 run "fanet-csma-simulation \
    --nNodes=${n} \
    --mobile=false \
    --rtsCtsThreshold=2346 \
    --csvFile=results/csv/sweep-static-rts-off.csv \
    --flowXmlFile=results/flowmon/n${n}-static-rts-off.flowmon.xml"
done
```

Static, RTS/CTS bật:

```bash
for n in 2 5 10 15 20 25 30; do
  ./ns3 run "fanet-csma-simulation \
    --nNodes=${n} \
    --mobile=false \
    --rtsCtsThreshold=0 \
    --csvFile=results/csv/sweep-static-rts-on.csv \
    --flowXmlFile=results/flowmon/n${n}-static-rts-on.flowmon.xml"
done
```

### 9.2. Sweep đầy đủ 2..30

Mobile, RTS/CTS tắt:

```bash
for n in $(seq 2 30); do
  ./ns3 run "fanet-csma-simulation \
    --nNodes=${n} \
    --mobile=true \
    --rtsCtsThreshold=2346 \
    --csvFile=results/csv/full-mobile-rts-off.csv \
    --flowXmlFile=results/flowmon/full-n${n}-mobile-rts-off.flowmon.xml"
done
```

Nếu muốn so sánh đầy đủ 4 kịch bản:

```bash
for mobile in true false; do
  for rts in 2346 0; do
    for n in $(seq 2 30); do
      if [ "$mobile" = "true" ]; then mode="mobile"; else mode="static"; fi
      if [ "$rts" = "0" ]; then rtsname="rts-on"; else rtsname="rts-off"; fi
      ./ns3 run "fanet-csma-simulation \
        --nNodes=${n} \
        --mobile=${mobile} \
        --rtsCtsThreshold=${rts} \
        --csvFile=results/csv/full-${mode}-${rtsname}.csv \
        --flowXmlFile=results/flowmon/full-n${n}-${mode}-${rtsname}.flowmon.xml"
    done
  done
done
```

---

## 10. Vẽ Đồ Thị Từ CSV

Cài thư viện:

```bash
pip install pandas matplotlib
```

Ví dụ vẽ PDR theo `nNodes` cho mobile RTS off/on:

```python
import pandas as pd
import matplotlib.pyplot as plt

off = pd.read_csv("results/csv/sweep-mobile-rts-off.csv")
on = pd.read_csv("results/csv/sweep-mobile-rts-on.csv")

plt.figure(figsize=(9, 5))
plt.plot(off["nNodes"], off["pdrPercent"], "r-o", label="Mobile RTS/CTS OFF")
plt.plot(on["nNodes"], on["pdrPercent"], "g-s", label="Mobile RTS/CTS ON")
plt.xlabel("Number of nodes")
plt.ylabel("PDR (%)")
plt.title("PDR vs Number of Nodes")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig("results/csv/pdr-mobile-rts-comparison.png", dpi=150)
plt.show()
```

Ví dụ vẽ throughput:

```python
plt.figure(figsize=(9, 5))
plt.plot(off["nNodes"], off["throughputMbps"], "r-o", label="Mobile RTS/CTS OFF")
plt.plot(on["nNodes"], on["throughputMbps"], "g-s", label="Mobile RTS/CTS ON")
plt.xlabel("Number of nodes")
plt.ylabel("Throughput (Mbps)")
plt.title("Throughput vs Number of Nodes")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig("results/csv/throughput-mobile-rts-comparison.png", dpi=150)
plt.show()
```

---

## 11. Diễn Giải Kết Quả

### PDR

```text
PDR = rxPackets / txPackets * 100%
```

PDR cao nghĩa là phần lớn gói tin tới Gateway thành công.

Nếu PDR giảm khi `nNodes` tăng:

- Kênh WiFi bị tranh chấp nhiều hơn.
- Collision tăng.
- Hidden node có thể xuất hiện.
- Drone có thể nằm xa Gateway hơn.

### Throughput

```text
Throughput = rxPayloadBytes * 8 / simTime / 1,000,000
```

Throughput là dữ liệu payload thực tế Gateway nhận được, không phải offered load.

### Packet loss

```text
Packet loss = 100 - PDR
```

Packet loss cao cho thấy mạng không ổn định hoặc quá tải.

### Average delay

Delay tăng khi:

- Nhiều node tranh kênh.
- MAC backoff nhiều.
- Có retry.
- Wireless link yếu.

### Link tearing rate

```text
Link tearing rate = Gateway range exit events / simTime
```

Chỉ số này phản ánh tính di động: drone ra khỏi vùng Gateway càng nhiều thì link tearing càng cao.

---

## 12. Lỗi Thường Gặp

### 12.1. Lỗi do dùng option README GitHub cũ

Nếu chạy:

```bash
./ns3 run "fanet-csma-simulation --writeNetAnim=true"
```

hoặc:

```bash
./ns3 run "fanet-csma-simulation --enablePcap=true"
```

bản compact sẽ lỗi vì không còn option này.

Cách xử lý: bỏ các option NetAnim/PCAP/milestone.

### 12.2. CSV bị trộn nhiều lần chạy

CSV dùng append. Nếu không muốn trộn, đặt file riêng:

```bash
./ns3 run "fanet-csma-simulation --csvFile=results/csv/test-run.csv"
```

### 12.3. XML bị ghi đè

Nếu không đặt `flowXmlFile`, XML mặc định chỉ phân biệt theo `nNodes` và `mode`, không phân biệt RTS.

Cách xử lý:

```bash
--flowXmlFile=results/flowmon/n30-mobile-rts-on.flowmon.xml
```

### 12.4. Throughput bằng 0

Không kết luận ngay là code sai. Hãy test nhỏ:

```bash
./ns3 run "fanet-csma-simulation --nNodes=3 --simTime=10 --areaSide=50"
```

Nếu test nhỏ nhận gói được, nhưng bản 500m không nhận được, lý do thường là:

- Drone quá xa Gateway.
- Path loss exponent cao.
- Wireless link yếu.
- Mobility làm node ra khỏi vùng liên lạc.

### 12.5. Muốn tái lập kết quả

Dùng cùng `rngRun`:

```bash
./ns3 run "fanet-csma-simulation --rngRun=1"
```

Đổi `rngRun` để lấy nhiều mẫu thống kê:

```bash
./ns3 run "fanet-csma-simulation --rngRun=2"
```

---

## 13. Những Điểm Khác README GitHub Gốc

README GitHub gốc có hướng dẫn:

- Milestone CSV.
- NetAnim.
- PCAP.
- `--milestoneCsvFile`.
- `--writeNetAnim`.
- `--enablePcap`.

Bản compact không có các phần đó. Vì vậy README này đã sửa lại:

- Chỉ hướng dẫn CSV summary.
- Chỉ hướng dẫn FlowMonitor XML.
- Giữ output console giống style bản GitHub.
- Giữ các kịch bản mobile/static và RTS on/off.
- Thêm lưu ý tránh ghi đè XML khi so sánh RTS.

---

## 14. Checklist Trước Khi Nộp

Kiểm tra source:

```bash
ls scratch/fanet-csma-simulation.cc
```

Kiểm tra build:

```bash
./ns3 build
```

Kiểm tra chạy nhanh:

```bash
./ns3 run "fanet-csma-simulation --nNodes=3 --simTime=10 --areaSide=50"
```

Chạy 4 kịch bản chính:

```bash
./ns3 run "fanet-csma-simulation --mobile=true  --rtsCtsThreshold=2346 --csvFile=results/csv/mobile-rts-off.csv --flowXmlFile=results/flowmon/mobile-rts-off.xml"
./ns3 run "fanet-csma-simulation --mobile=true  --rtsCtsThreshold=0    --csvFile=results/csv/mobile-rts-on.csv  --flowXmlFile=results/flowmon/mobile-rts-on.xml"
./ns3 run "fanet-csma-simulation --mobile=false --rtsCtsThreshold=2346 --csvFile=results/csv/static-rts-off.csv --flowXmlFile=results/flowmon/static-rts-off.xml"
./ns3 run "fanet-csma-simulation --mobile=false --rtsCtsThreshold=0    --csvFile=results/csv/static-rts-on.csv  --flowXmlFile=results/flowmon/static-rts-on.xml"
```

Kiểm tra kết quả:

```bash
ls results/csv
ls results/flowmon
```

Nếu cần vẽ biểu đồ theo số node, chạy sweep ở mục 9.

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
