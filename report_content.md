# Nội dung Report: Mô phỏng Hiệu năng CSMA/CA trong Mạng FANET

---

## 3. Thiết Kế Kịch Bản Mô Phỏng (Scenario Design)

### 3.1. Mục tiêu mô phỏng

Mô phỏng được xây dựng nhằm đánh giá hiệu năng của giao thức CSMA/CA trong môi trường mạng FANET (Flying Ad-hoc Network) khi số lượng UAV tăng dần. Cụ thể, thí nghiệm tập trung phân tích tác động của vấn đề **hidden node** và **xung đột kênh truyền ở tầng MAC** lên ba chỉ tiêu: thông lượng mạng (Throughput), tỉ lệ phân phát gói tin thành công (PDR) và trễ truyền đầu cuối trung bình (Average End-to-End Delay). Để cô lập hoàn toàn hiệu ứng MAC, không có mô hình suy giảm tín hiệu vật lý (fading) hay giao thức định tuyến đa chặng nào được kích hoạt.

### 3.2. Topology Mạng

#### 3.2.1. Số lượng node

Mô phỏng sử dụng **mô hình many-to-one** gồm hai thành phần:

- **1 node Sink** (Ground Station): đóng vai trò điểm thu duy nhất, đặt cố định tại tâm vùng mô phỏng.
- **n node UAV**: đóng vai trò nguồn phát, di chuyển ngẫu nhiên trong vùng mô phỏng.

Thực nghiệm được lặp lại với bốn mức mật độ UAV: **n = 4, 9, 19, 29**, tương ứng tổng số node lần lượt là 5, 10, 20, 30. Việc tăng dần số UAV cho phép quan sát quá trình suy giảm hiệu năng khi kênh truyền tiến đến trạng thái bão hoà.

#### 3.2.2. Giao thức và ngăn xếp mạng

Ngăn xếp giao thức được lựa chọn theo các tầng OSI như sau:

| Tầng | Giao thức / Công nghệ | Lý do lựa chọn |
|---|---|---|
| **Vật lý (PHY)** | IEEE 802.11b, 2.4 GHz | Chuẩn phổ biến cho thiết bị không dây cỡ nhỏ; tốc độ tối đa 11 Mbps giúp bộc lộ bão hoà rõ ràng |
| **Liên kết dữ liệu (MAC)** | CSMA/CA Ad-Hoc (không có AP) | Phản ánh đặc tính phi tập trung của FANET; không có cơ chế điều phối trung tâm |
| **Mạng (Network)** | IPv4 (không có routing protocol) | Giao tiếp đơn chặng trực tiếp từ UAV đến Sink; không cần định tuyến đa chặng |
| **Giao vận (Transport)** | UDP | Phù hợp với luồng dữ liệu thời gian thực (telemetry, video); không có cơ chế kiểm soát tắc nghẽn can thiệp vào kết quả đo |
| **Ứng dụng** | CBR (Constant Bit Rate) | Tạo tải ổn định, có thể kiểm soát; dễ tính toán và so sánh kết quả |

#### 3.2.3. Điều khiển cơ chế RTS/CTS

Thí nghiệm bao gồm hai chế độ MAC để so sánh:

- **Chế độ không có RTS/CTS** *(mặc định)*: Kênh truy cập trực tiếp theo CSMA/CA thuần túy. Các node UAV cạnh tranh kênh truyền mà không có bước trao đổi RTS/CTS bảo vệ, tối đa hoá xác suất xung đột — đặc biệt nghiêm trọng khi có hidden node.
- **Chế độ có RTS/CTS**: Trước khi gửi data frame, node phát đi gói RTS (Request to Send); sau khi Sink phản hồi bằng CTS (Clear to Send), các node khác trong vùng nghe được sẽ tạm ngừng phát. Cơ chế này làm giảm đáng kể xung đột nhưng tốn thêm overhead.

### 3.3. Mô hình di động (Mobility Model)

#### 3.3.1. Node Sink — Vị trí cố định

Node Sink (Node 0) được đặt **cố định tại tọa độ (50, 50, 0)** — tâm hình học của vùng mô phỏng hình vuông 100m × 100m. Vị trí này đảm bảo khoảng cách trung bình từ mọi UAV đến Sink là tương đương nhau, tránh sai lệch do vị trí lệch tâm. Mô hình được sử dụng: `ConstantPositionMobilityModel`.

#### 3.3.2. Node UAV — Di chuyển ngẫu nhiên

Các node UAV di chuyển theo mô hình **Random Walk 2D** trong vùng hình chữ nhật 100m × 100m với các thông số:

| Thông số | Giá trị |
|---|---|
| Vị trí khởi đầu | Phân bố đều ngẫu nhiên trong `[0, 100] × [0, 100]` m |
| Quỹ đạo di chuyển | Ngẫu nhiên, đổi hướng khi chạm biên vùng mô phỏng |
| Tốc độ | Phân bố đều `Uniform[10, 20]` m/s |
| Biên | Hình chữ nhật `(0, 100, 0, 100)` m |

Mô hình Random Walk được chọn vì tính đơn giản và khả năng tạo ra tính ngẫu nhiên cao trong vị trí tương đối giữa các node — điều kiện lý tưởng để tái hiện vấn đề hidden node một cách tự nhiên mà không cần định nghĩa quỹ đạo cứng nhắc.

Vùng mô phỏng **100m × 100m** được chọn đủ nhỏ để các node thường xuyên nằm trong phạm vi truyền của nhau, đồng thời tạo xác suất cao xuất hiện hidden node — là điều kiện cần thiết để thấy sự khác biệt giữa hai chế độ RTS/CTS.

### 3.4. Phát sinh lưu lượng (Application Generation)

#### 3.4.1. Định nghĩa sender và receiver

- **Sender (nguồn phát):** Toàn bộ `n` node UAV, mỗi node phát độc lập đến cùng một đích.
- **Receiver (đích nhận):** Duy nhất node Sink (địa chỉ IP: 10.1.1.1).

Mô hình **many-to-one** này tái hiện tình huống thực tế của FANET: nhiều UAV đồng thời truyền dữ liệu thu thập được (ảnh, cảm biến, telemetry) về trạm mặt đất.

#### 3.4.2. Thông số tạo lưu lượng

| Thông số | Giá trị | Mục đích |
|---|---|---|
| Loại traffic | UDP CBR (Constant Bit Rate) | Tải ổn định, không bị ảnh hưởng bởi cơ chế TCP congestion control |
| Kích thước gói (PacketSize) | 512 bytes | Đại diện cho gói telemetry / video stream kích thước nhỏ điển hình |
| Tốc độ gửi (Interval) | 0.01 giây (100 gói/giây/UAV) | Tạo tải cao; kết hợp với 29 UAV vượt giới hạn kênh truyền |
| Thời gian bắt đầu gửi | t = 1 giây | Đảm bảo Sink đã sẵn sàng trước khi nhận gói đầu tiên |
| Thời gian kết thúc | t = 50 giây | |
| Tổng tải (nUavs = 29) | ≈ 11.9 Mbps | Vượt giới hạn lý thuyết 802.11b (11 Mbps) → gây bão hoà MAC có chủ ý |

Tải được thiết kế **có chủ ý vượt mức** kênh truyền khi số UAV lớn nhằm quan sát điểm suy giảm hiệu năng. Khi `nUavs = 4`, tổng tải ≈ 1.6 Mbps — kênh truyền còn dư thừa và PDR dự kiến cao. Khi `nUavs = 29`, kênh truyền bị bão hoà hoàn toàn và hiện tượng sụp đổ hiệu năng (performance collapse) sẽ thể hiện rõ.

---

## 4. Triển Khai Mô Phỏng (Implementation)

### 4.1. Công cụ và môi trường

Mô phỏng được triển khai bằng ngôn ngữ **C++** trên nền tảng **ns-3 phiên bản 3.39** — công cụ mô phỏng mạng mã nguồn mở dựa trên cơ chế sự kiện rời rạc (discrete-event simulation). File mã nguồn chính: `scratch/scratch-simulator.cc`.

Lệnh biên dịch và chạy:
```bash
./ns3 run "scratch-simulator --nUavs=<n> --enableRts=<true|false>"
```

### 4.2. Tham số điều khiển

Chương trình nhận hai tham số dòng lệnh được khai báo qua `CommandLine` của ns-3:

```cpp
uint32_t nUavs    = 4;      // Mặc định: 4 UAVs
bool     enableRts = false; // Mặc định: tắt RTS/CTS

CommandLine cmd(__FILE__);
cmd.AddValue("nUavs",     "Số UAV node", nUavs);
cmd.AddValue("enableRts", "Bật/tắt RTS/CTS", enableRts);
cmd.Parse(argc, argv);
```

Bảng tổng hợp tất cả tham số mô phỏng:

| Tham số | Kiểu | Giá trị | Cách đặt |
|---|---|---|---|
| `nUavs` | `uint32_t` | 4, 9, 19, 29 | Dòng lệnh `--nUavs` |
| `enableRts` | `bool` | `true` / `false` | Dòng lệnh `--enableRts` |
| `simTime` | `double` | 50.0 giây | Cố định trong code |
| `PacketSize` | `uint32_t` | 512 bytes | Attribute của `UdpClient` |
| `Interval` | `Time` | 0.01 giây | Attribute của `UdpClient` |
| `MaxPackets` | `uint32_t` | 4,294,967,295 | Attribute của `UdpClient` (vô hạn) |
| Vùng mô phỏng | — | 100m × 100m | Attribute của Mobility |
| Tốc độ UAV | — | Uniform[10, 20] m/s | Attribute của RandomWalk2d |

### 4.3. Khởi tạo node mạng

```cpp
NodeContainer allNodes;
allNodes.Create(1 + nUavs);              // Tạo tổng (1 + nUavs) node

Ptr<Node> sinkNode = allNodes.Get(0);   // Node 0 = Sink

NodeContainer uavNodes;
for (uint32_t i = 1; i < 1 + nUavs; ++i)
    uavNodes.Add(allNodes.Get(i));       // Node 1..N = UAVs
```

Node Sink và các node UAV được tách vào hai `NodeContainer` riêng biệt vì chúng cần được cấu hình mobility khác nhau ở bước sau.

### 4.4. Cấu hình tầng Wi-Fi

#### 4.4.1. Điều khiển RTS/CTS

Ngưỡng RTS/CTS được thiết lập thông qua attribute toàn cục của ns-3, **bắt buộc gọi trước** lệnh `wifi.Install()`:

```cpp
Config::SetDefault(
    "ns3::WifiRemoteStationManager::RtsCtsThreshold",
    StringValue(enableRts ? "0" : "999999")
);
```

- Giá trị `"0"`: Mọi data frame đều được bảo vệ bởi RTS/CTS (luôn bật).
- Giá trị `"999999"`: Kích thước ngưỡng lớn hơn mọi frame → RTS/CTS không bao giờ được kích hoạt (hoàn toàn tắt).

#### 4.4.2. Tầng vật lý (PHY)

```cpp
YansWifiPhyHelper   wifiPhy;
YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
wifiPhy.SetChannel(wifiChannel.Create());
```

Module `YansWifiPhy` (Yet Another Network Simulator Wi-Fi PHY) được sử dụng với cấu hình kênh mặc định áp dụng mô hình suy hao `LogDistancePropagationLossModel`. Việc không bổ sung mô hình fading (Rayleigh, Nakagami) là có chủ ý: mọi suy giảm hiệu năng quan sát được chỉ phản ánh xung đột MAC, loại bỏ nhiễu từ tầng vật lý.

#### 4.4.3. Tầng liên kết dữ liệu (MAC)

```cpp
WifiHelper wifi;
wifi.SetStandard(WIFI_STANDARD_80211b);
wifi.SetRemoteStationManager("ns3::AarfWifiManager");

WifiMacHelper wifiMac;
wifiMac.SetType("ns3::AdhocWifiMac");

NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);
```

| Lựa chọn | Module ns-3 | Ý nghĩa |
|---|---|---|
| Chuẩn Wi-Fi | `WIFI_STANDARD_80211b` | 802.11b, băng thông tối đa 11 Mbps |
| Chế độ MAC | `ns3::AdhocWifiMac` | Mạng ngang hàng, không có điểm truy cập (AP) |
| Rate control | `ns3::AarfWifiManager` | Tự động điều chỉnh tốc độ truyền theo chất lượng kênh (Adaptive ARF) |

### 4.5. Cài đặt ngăn xếp Internet

```cpp
InternetStackHelper internet;
internet.Install(allNodes);                       // Cài IPv4/UDP trên tất cả node

Ipv4AddressHelper ipv4;
ipv4.SetBase("10.1.1.0", "255.255.255.0");
Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
```

Không cài đặt thêm giao thức định tuyến (AODV, OLSR) vì toàn bộ giao tiếp là **đơn chặng trực tiếp** từ UAV đến Sink. Địa chỉ IP của Sink là `10.1.1.1` (node đầu tiên được gán).

### 4.6. Cấu hình mô hình di động

#### 4.6.1. Sink — cố định tại tâm

```cpp
Ptr<ListPositionAllocator> sinkPos = CreateObject<ListPositionAllocator>();
sinkPos->Add(Vector(50.0, 50.0, 0.0));

MobilityHelper sinkMobility;
sinkMobility.SetPositionAllocator(sinkPos);
sinkMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
sinkMobility.Install(sinkNode);
```

#### 4.6.2. UAVs — Random Walk 2D

```cpp
MobilityHelper uavMobility;
uavMobility.SetPositionAllocator(
    "ns3::RandomRectanglePositionAllocator",
    "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
    "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

uavMobility.SetMobilityModel(
    "ns3::RandomWalk2dMobilityModel",
    "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
    "Speed", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=20.0]"));

uavMobility.Install(uavNodes);
```

Vị trí khởi đầu của mỗi UAV được phân bổ độc lập và đồng đều trong toàn bộ vùng 100m × 100m theo phân bố `UniformRandomVariable`. Trong quá trình mô phỏng, mỗi UAV chọn hướng di chuyển ngẫu nhiên và duy trì cho đến khi chạm biên vùng, sau đó phản xạ và tiếp tục theo hướng mới.

### 4.7. Cài đặt ứng dụng phát sinh lưu lượng

#### 4.7.1. UdpServer — phía Sink

```cpp
UdpServerHelper udpServer(9);   // Lắng nghe cổng UDP 9
ApplicationContainer serverApp = udpServer.Install(sinkNode);
serverApp.Start(Seconds(0.0));
serverApp.Stop(Seconds(50.0));
```

`UdpServer` là ứng dụng thu nhận đơn giản: ghi nhận số gói đến và thời gian nhận, không gửi phản hồi. Khởi động tại `t = 0s` để đảm bảo sẵn sàng trước khi UAV bắt đầu phát.

#### 4.7.2. UdpClient — phía UAV

```cpp
Ipv4Address sinkAddr = interfaces.GetAddress(0);  // 10.1.1.1

UdpClientHelper udpClient(sinkAddr, 9);
udpClient.SetAttribute("MaxPackets", UintegerValue(4294967295u));
udpClient.SetAttribute("Interval",   TimeValue(Seconds(0.01)));
udpClient.SetAttribute("PacketSize", UintegerValue(512));

ApplicationContainer clientApps = udpClient.Install(uavNodes);
clientApps.Start(Seconds(1.0));
clientApps.Stop(Seconds(50.0));
```

Mỗi `UdpClient` phát độc lập theo chu kỳ cố định 10ms (100 gói/giây), tạo ra luồng CBR hướng về địa chỉ Sink. Tất cả client bắt đầu đồng thời tại `t = 1s`, tạo tình huống nhiều node cùng cạnh tranh kênh truyền ngay từ đầu — điều kiện nghiêm khắc nhất cho CSMA/CA.

### 4.8. Thu thập và tính toán số liệu — FlowMonitor

#### 4.8.1. Cài đặt FlowMonitor

```cpp
FlowMonitorHelper flowMonHelper;
Ptr<FlowMonitor> flowMonitor = flowMonHelper.InstallAll();
```

`InstallAll()` gắn probe giám sát vào **mọi node** trong mô phỏng, cho phép theo dõi toàn bộ luồng IP đi qua mạng mà không cần khai báo từng flow riêng lẻ.

#### 4.8.2. Trích xuất số liệu sau mô phỏng

Sau khi `Simulator::Run()` kết thúc, dữ liệu được trích xuất và lọc theo địa chỉ đích:

```cpp
flowMonitor->CheckForLostPackets();   // Khai báo gói bị mất do bộ đệm

Ptr<Ipv4FlowClassifier> classifier =
    DynamicCast<Ipv4FlowClassifier>(flowMonHelper.GetClassifier());

for (auto& [flowId, stats] : flowMonitor->GetFlowStats()) {
    auto tuple = classifier->FindFlow(flowId);
    if (tuple.destinationAddress == sinkAddr) {   // Chỉ lấy flow đến Sink
        totalTxPackets += stats.txPackets;
        totalRxPackets += stats.rxPackets;
        totalRxBytes   += stats.rxBytes;
        totalDelaySum  += stats.delaySum.GetSeconds();
    }
}
```

Bộ lọc `destinationAddress == sinkAddr` loại bỏ các flow phụ (ARP request, traffic không liên quan) ra khỏi tính toán.

#### 4.8.3. Công thức tính chỉ tiêu hiệu năng

Ba chỉ tiêu hiệu năng chính được tính theo công thức:

$$\text{Throughput (Mbps)} = \frac{\text{totalRxBytes} \times 8}{\text{simTime} \times 10^6}$$

$$\text{PDR (\%)} = \frac{\text{totalRxPackets}}{\text{totalTxPackets}} \times 100$$

$$\text{Avg Delay (ms)} = \frac{\text{totalDelaySum (s)}}{\text{totalRxPackets}} \times 1000$$

- **Throughput** tính trên toàn bộ thời gian mô phỏng 50 giây (kể cả 1 giây khởi động không có traffic), phản ánh effective throughput thực tế.
- **PDR** là tỉ số giữa tổng gói nhận thành công và tổng gói đã gửi từ tất cả UAV, phản ánh mức độ thất thoát tổng thể do xung đột.
- **Avg E2E Delay** là trung bình của tổng thời gian truyền từ nguồn đến đích cho mỗi gói nhận thành công, bao gồm thời gian chờ MAC (backoff) và thời gian truyền qua kênh.
