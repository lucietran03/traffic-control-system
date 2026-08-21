# A. System Scope & Physical Map

**A1.** Mình giữ nguyên topology của map thầy 100%, đúng không?

**A2.** Mình có muốn base physical dimensions trên **một intersection/railway area thật** như đề khuyến khích, hay tự tạo một realistic fictional network? Đề khuyến khích ít nhất attempt dựa trên real intersection và dùng khoảng cách để derive timing. 

**A3.** Nếu real-world based: mình muốn tìm **một khu vực thật có railway + multiple intersections**, hay chỉ lấy real dimensions/traffic principles rồi giữ topology R1–R5 của thầy?

**A4.** Khoảng cách chính xác giữa:

* I1–I3?
* I3–I5?
* I2–I4?
* I4–I6?
* I1–RC1–I2?
* I3–RC2–I4?
* I5–RC3–I6?

**A5.** Có model minor roads/driveways/access roads không, hay exclude toàn bộ ngoài R1–R5?

**A6.** Speed limit trên R1–R5 là bao nhiêu? Giống nhau hay khác nhau?

**A7.** Có model vehicle travel time giữa intersections không?

**A8.** Có model physical queue capacity của road segments không? Ví dụ đoạn 60 m chứa tối đa khoảng X cars.

---

# B. Lane & Vehicle Movement Design

Đề chỉ freeze **2 lanes each direction**; turn-lane details chưa freeze. 

**B1.** Mỗi direction có 2 lanes rồi. Mỗi lane được phép làm gì?

Ví dụ chọn một trong:

```text
Option 1
Inner: Straight
Outer: Straight + Right

Option 2
Inner: Left + Straight
Outer: Straight + Right

Option 3
Inner: Left only
Outer: Straight + Right

Option 4
Different configuration depending on road/intersection
```

**B2.** Có support **left turn** không?

**B3.** Nếu có left turn: `permissive` (yield opposing traffic) hay `protected` bằng green arrow?

**B4.** Có dedicated right-turn arrow không?

**B5.** Right turn on red có được phép không?

**B6.** Tất cả I1–I6 có cùng lane configuration, hay major/minor intersections khác nhau?

**B7.** Có model turning traffic proportion không? Ví dụ mỗi approach:

* 70% straight
* 20% right
* 10% left

**B8.** Có model individual vehicles thật trong simulation, hay sensor/events đại diện traffic demand?

---

# C. Traffic-Light Hardware at I1–I6

**C1.** Mỗi intersection có bao nhiêu vehicle signal heads/groups?

**C2.** Mỗi approach dùng một logical signal group hay từng lane có signal riêng?

**C3.** Vehicle signal states gồm chính xác:
`RED / YELLOW / GREEN`?

**C4.** Có turn-arrow signals không?

**C5.** Có all-red clearance state giữa conflicting phases không?

**C6.** Có flashing-yellow/flashing-red emergency state không?

**C7.** Tất cả I1–I6 dùng cùng hardware configuration hay khác nhau?

---

# D. Pedestrian Design

Pedestrian buttons/signals được đề đặc biệt khuyến khích từ CR trở lên, nhưng exact design là của team. 

**D1.** Có pedestrian crossing ở cả 4 sides của mỗi I1–I6 không?

**D2.** Mỗi crossing có push button riêng?

**D3.** Có automatic pedestrian sensor ngoài button không?

**D4.** Button press được latch cho tới khi served?

**D5.** Press 10 lần có khác press 1 lần không?

**D6.** Pedestrian được phục vụ ở Peak lẫn Off-Peak?

**D7.** Pedestrian phase:

* chạy cùng non-conflicting vehicle traffic,
* hay toàn bộ vehicles red?

**D8.** WALK kéo dài bao lâu?

**D9.** Có `WALK → FLASHING DON'T WALK → DON'T WALK` không, hay chỉ `GREEN/RED pedestrian`?

**D10.** Maximum pedestrian waiting time là bao nhiêu?

**D11.** Nếu pedestrian request đến ngay khi train event xảy ra thì giữ request hay discard?

**D12.** Nếu pedestrian button hỏng thì fallback thế nào?

---

# E. Road Traffic Demand

Đề explicitly bắt team **state and justify differences in demand/priority R1–R5**. 

**E1.** Mình có bao nhiêu traffic-demand modes?

* Peak + Off-Peak?
* Peak + Interpeak + Night?
* Morning Peak + Day + Evening Peak + Night?

**E2.** Exact time windows là gì?

**E3.** R1–R5 mỗi road có bao nhiêu `veh/h/direction` trong từng period?

**E4.** Demand hai chiều trên cùng road bằng nhau hay khác nhau?

**E5.** Morning peak có directional bias không? Ví dụ inbound 70%, outbound 30%.

**E6.** Evening peak có reverse directional bias không?

**E7.** R1–R5 đóng vai trò gì?

* arterial,
* collector,
* railway connector,
* local road?

**E8.** Có traffic demand random variation hay dùng fixed assumed rates?

**E9.** Traffic arrival model cần realistic đến đâu? Constant / random / burst?

---

# F. Road Priority

**F1.** R1–R5 priority order là gì?

**F2.** Priority dựa trên cái gì:

* traffic volume,
* road class,
* railway proximity,
* queue risk,
* combination?

**F3.** Có muốn dùng numerical priority `1–5` không?

**F4.** Priority static hay thay đổi Peak/Off-Peak?

**F5.** `Traffic priority` và `coordination sensitivity` có tách riêng không?

Ví dụ R1 đông nhất nhưng R4 gần railway nhất.

**F6.** Lower-priority road được phép chờ tối đa bao lâu?

**F7.** Có anti-starvation rule không?

---

# G. Vehicle Sensors

Đề cho sensor-driven mode phản ứng với local car sensors. 

**G1.** Sensor nằm ở đâu?

**G2.** Một sensor/approach hay một sensor/lane?

**G3.** Sensor detect:

* presence only,
* vehicle count,
* arrival rate,
* queue length?

**G4.** Có một sensor gần stop line và một sensor xa hơn để estimate queue không?

**G5.** Sensor input persistent hay event-based?

**G6.** Sensor dùng trong Off-Peak để quyết định phase?

**G7.** Peak fixed timing thì sensor:

* ignored,
* monitoring only,
* hay vẫn được phép extend green?

**G8.** Sensor có dùng để detect congestion không?

**G9.** Sensor failure được detect bằng cách nào?

**G10.** Sensor hỏng → fixed timing fallback?

**G11.** Một sensor hỏng có affect cả intersection hay chỉ approach đó?

---

# H. Normal Traffic-Light Sequence

Đề cho ba family: fixed timing, sensor driven, advanced/updateable sequence. 

**H1.** Một normal cycle có những phases nào?

**H2.** Opposite directions có green đồng thời không?

**H3.** Yellow duration?

**H4.** All-red clearance duration?

**H5.** Minimum green?

**H6.** Maximum green?

**H7.** Fixed-timing cycle tổng bao nhiêu giây?

**H8.** Green allocation cho major/minor roads bao nhiêu?

**H9.** Sensor-driven mode bắt đầu phase dựa trên rule nào?

**H10.** Sensor-driven green được extend theo từng bao nhiêu giây?

**H11.** Khi không có traffic ở bất kỳ approach nào thì intersection giữ state gì?

**H12.** Khi tất cả approaches đều demand thì chọn ai trước?

**H13.** Khi đổi Fixed ↔ Sensor-driven có đợi hết cycle không?

**H14.** Demo có speed-up timing không? Đề cho phép timing được speed up để demonstration. 

**H15.** Nếu speed-up, scale factor bao nhiêu và relationship với real timing thế nào?

---

# I. Central vs Local Responsibility

Đây là cực quan trọng: Central **không trực tiếp điều khiển individual lights**; local làm việc đó. 

**I1.** Local controller tự chịu trách nhiệm những decision nào?

**I2.** Central được phép command những gì?

Ví dụ:

* change operating mode,
* change timing profile,
* change sequence,
* override request.

**I3.** Central gửi `GREEN` trực tiếp được không? → Theo brief thì **không**; vậy command abstraction của mình là gì?

**I4.** Central quyết định Peak/Off-Peak mode hay local tự biết clock?

**I5.** Central có thể send timing parameters không?

**I6.** Central có thể send completely new sequence structure không?

**I7.** Local có quyền reject Central command nếu unsafe không?

**I8.** Khi command đến giữa cycle, apply ngay hay next safe boundary?

**I9.** Local gửi status khi nào? Đề bắt gửi khi light state thay đổi.  Có gửi periodic heartbeat thêm không?

**I10.** Central display những information nào?

---

# J. Coordination Between Intersections

**J1.** I1–I3–I5 có coordinate thành corridor trên R1 không?

**J2.** I2–I4–I6 có coordinate trên R2 không?

**J3.** R3/R4/R5 có coordinate I-left ↔ I-right qua railway không?

**J4.** Có green-wave không?

**J5.** Nếu có, target travel speed bao nhiêu?

**J6.** Offset giữa intersections derive từ distance thế nào?

**J7.** Central tính coordination hay dùng predefined profiles?

**J8.** Local controllers có communicate trực tiếp với nhau không, hay tất cả qua C1?

**J9.** Coordination failure → intersection trở lại standalone sequence?

**J10.** Railway event phá green-wave thì sau train mình:

* restart,
* resynchronise gradually,
* hay immediately restore offsets?

---

# K. Congestion Definition & Control

**K1.** System định nghĩa “congestion” bằng cái gì?

**K2.** Exact threshold là bao nhiêu?

* X vehicles,
* sensor occupied X seconds,
* queue > X metres?

**K3.** Có `Normal / Heavy / Congested / Critical` không?

**K4.** Làm sao estimate queue nếu sensor chỉ binary?

**K5.** Queue capacity của từng segment là bao nhiêu vehicles?

**K6.** Khi congestion detected, local làm gì?

**K7.** Central làm gì?

**K8.** Có extend green không?

**K9.** Maximum extension bao nhiêu?

**K10.** Có shorten conflicting phase không?

**K11.** Minimum green của conflicting road vẫn phải bảo vệ?

**K12.** Có prevent queue entering railway crossing không?

**K13.** Nếu downstream road full thì upstream signal có giữ red để tránh blocking crossing/intersection không?

**K14.** Congestion intervention kết thúc khi threshold xuống bao nhiêu?

**K15.** Có hysteresis để tránh mode flip liên tục không?

---

# L. Railway Physical Design

Đề freeze double-track, opposite directions, boom gates + red flashing lights; exact architecture còn nhiều thứ để team quyết. 

**L1.** Có chính xác 3 crossings `RC1–RC3`?

**L2.** Mỗi RC có dedicated local controller không? Theo lecturer clarification của m thì có — muốn freeze thành `RL1–RL3`?

**L3.** Mỗi crossing có mấy boom gates?

**L4.** Gate placement thế nào?

**L5.** Mỗi crossing có mấy flashing-light units?

**L6.** Có road-facing warning lights cả hai phía?

**L7.** Train signal nằm ở đâu mỗi direction?

**L8.** Có một train signal cho mỗi track/direction?

**L9.** Railway controller quản lý:

* gates,
* flashers,
* train approach signals,
  hay train signal thuộc train-line controller riêng?

---

# M. Train Detection & Railway Sequence

**M1.** System biết train approaching bằng cách nào?

**M2.** Có `approach sensor`, `crossing sensor`, `exit sensor` không?

**M3.** Sensor nằm cách crossing bao nhiêu mét?

**M4.** Train speed assumed bao nhiêu?

**M5.** Train detected trước crossing bao nhiêu giây?

**M6.** Sequence có phải:

```text
Train detected
→ flash lights
→ gate closing
→ gate closed
→ train permitted
→ train enters
→ train exits
→ gate opens
→ road traffic resumes
```

hay khác?

**M7.** Flashing lights phải chạy bao lâu trước khi gate bắt đầu close?

**M8.** Gate closing time bao nhiêu?

**M9.** Gate phải confirmed fully closed bao lâu trước train arrival?

**M10.** Train signal default GREEN hay RED?

**M11.** Train chỉ được green khi gate confirmed closed?

**M12.** Sau train exit, delay bao lâu mới gate open?

**M13.** Hai trains opposite directions tới gần nhau thì gate giữ closed xuyên suốt?

**M14.** Train thứ hai cách train đầu bao lâu thì không reopen gate?

---

# N. Train Frequency

Brief cho `as little as 2 min` peak, khoảng `20 min` lúc ~10 PM, và Friday/Saturday có all-night trains. 

**N1.** Mình convert cái range này thành exact simulation schedule thế nào?

**N2.** Peak train headway = exactly 2 min?

**N3.** Off-Peak daytime = bao nhiêu?

**N4.** Evening = bao nhiêu?

**N5.** Night = 20 min?

**N6.** Mon–Thu/Sun railway service dừng lúc mấy giờ, nếu mình cần assume?

**N7.** Fri/Sat all-night headway bao nhiêu?

**N8.** Train arrival deterministic hay random trong range?

**N9.** Road Peak và Railway Peak có trùng nhau không?

---

# O. Railway ↔ Road Traffic Interaction

Đây là phần rất quan trọng mà mình chưa nên assume vội.

**O1.** Khi RC1 chuẩn bị close, những intersections nào cần biết?

**O2.** Chỉ I1/I2 hay cả network?

**O3.** RL1 gửi crossing state:

* trực tiếp L1/L2,
* qua C1,
* hay cả hai?

**O4.** Traffic controller cần biết states nào:
`OPEN / WARNING / CLOSING / CLOSED / TRAIN_PRESENT / OPENING / FAULT`?

**O5.** Khi gate chuẩn bị close, road traffic hướng **toward railway** có chuyển red ngay không?

**O6.** Hay phải finish current green/yellow/all-red safely trước?

**O7.** Traffic hướng **away from railway** có được ưu tiên green để clear trapped queue không?

**O8.** Có “clearance phase” đặc biệt trước train arrival không?

**O9.** Clearance phải bắt đầu trước train bao nhiêu giây?

**O10.** Làm sao đảm bảo không có vehicle trapped trên railway?

**O11.** Khi gate closed, adjacent intersections có tiếp tục service movements không dẫn vào railway?

**O12.** Khi gate reopen, normal cycle resume từ đầu hay resume previous phase?

**O13.** Có recovery phase để drain accumulated queues?

---

# P. Railway Fault Behaviour

**P1.** Boom gate có những failure modes nào?

* stuck open,
* stuck closed,
* unknown position?

**P2.** Flashing-light failure có model không?

**P3.** Train sensor failure có model không?

**P4.** Railway-controller failure có model không?

**P5.** Nếu gate không confirmed closed → train signal RED?

**P6.** Ai command train RED:

* RL,
* train-line controller,
* C1?

**P7.** Fault report đi đường nào tới C1/control room?

**P8.** Safety action có được phụ thuộc vào C1 không?

**P9.** Gate stuck closed nhưng không có train → road traffic làm gì?

**P10.** Gate stuck open + train approaching → adjacent traffic lights làm gì?

**P11.** Railway fault có override mọi normal traffic priority?

**P12.** Operator có được manually clear railway fault không?

---

# Q. Override / Exceptional Situations

Đề explicitly cho Central initiate override, ví dụ clear route cho dignitary. 

**Q1.** Team support override không?

**Q2.** Override types nào?

**Q3.** `CLEAR_ROUTE` nghĩa cụ thể gì?

**Q4.** Central gửi high-level override command hay sequence parameters?

**Q5.** Local vẫn phải finish safe transition trước khi apply override?

**Q6.** Override có thể interrupt pedestrian phase đang WALK không?

**Q7.** Override có thể override railway protection không? (Tao recommend tuyệt đối không, nhưng team phải formally decide.)

**Q8.** Override duration tối đa?

**Q9.** Override hết thì resume normal operation thế nào?

**Q10.** Operator command invalid/unsafe → local reject?

---

# R. Safety Priority Hierarchy

Cái này cuối cùng tụi m phải **chốt một hierarchy rõ ràng**.

**R1.** Event nào priority cao nhất?

**R2.** Railway fault vs train approaching?

**R3.** Active pedestrian clearance vs Central override?

**R4.** Emergency/override vs normal congestion?

**R5.** Congestion vs fixed timing?

**R6.** High-priority road vs lower road starvation?

**R7.** Có muốn hierarchy dạng:

```text
1. Hardware/safety fault
2. Railway protection
3. Active pedestrian/vehicle clearance
4. Exceptional operator override
5. Congestion prevention
6. Pedestrian request
7. Normal traffic priority
8. Normal timing optimisation
```

hay team muốn order khác?

---

# S. Communication Architecture

Đề yêu cầu multiple QNX nodes và IPC; local-central communication là core architecture. 

**S1.** Một logical controller = một QNX node?

**S2.** Nếu 9 locals + C1 thì implementation thật sự simulate bao nhiêu nodes?

**S3.** Full design và PoC implementation có khác node count không?

**S4.** Local ↔ Central dùng QNX message passing/Qnet?

**S5.** Railway local ↔ traffic local communicate trực tiếp không?

**S6.** Có heartbeat không?

**S7.** Heartbeat interval bao nhiêu?

**S8.** Bao lâu không heartbeat thì declare communication lost?

**S9.** Message acknowledgement cần cho commands nào?

**S10.** Có timeout/retry không?

**S11.** Retry bao nhiêu lần?

**S12.** Duplicate command xử lý sao?

**S13.** Out-of-order/stale message xử lý sao?

---

# T. Status Reporting

**T1.** Ngoài mandatory light-state changes, local gửi status gì nữa?

**T2.** Status gồm:

* current phase,
* individual lights,
* operating mode,
* sensor states,
* pending pedestrian requests,
* congestion,
* railway state,
* faults?

**T3.** Event-driven only hay periodic snapshot nữa?

**T4.** Central lưu history không?

**T5.** Central có detect stale local status không?

**T6.** Railway controllers report những states nào?

---

# U. Communication / Controller Failure

**U1.** C1 offline → locals tiếp tục mode nào?

**U2.** Dùng last valid configuration hay default fixed timing?

**U3.** Local mất connection với C1 nhưng vẫn có sensor → sensor-driven tiếp tục được không?

**U4.** Hai local mất communication với nhau có ảnh hưởng không?

**U5.** Khi communication restore, ai initiate resync?

**U6.** Local gửi actual state trước khi nhận coordination mới?

**U7.** Local-controller software failure → lights làm gì?

**U8.** Hardware light failure → detect/model không?

**U9.** Invalid Central command → reject/report?

**U10.** Central có single point of failure đối với **safety** không? Theo brief locals phải tiếp tục hoạt động khi central/comms fail. 

---

# V. Time & Real-Time Requirements

**V1.** Những events nào có hard deadline?

**V2.** Train approach → traffic-safe state deadline bao nhiêu?

**V3.** Sensor event → controller response deadline?

**V4.** Central command → acknowledgement deadline?

**V5.** Light-state change → Central status report deadline?

**V6.** Pedestrian maximum response/wait?

**V7.** Communication timeout?

**V8.** Timing jitter acceptable bao nhiêu?

**V9.** Deadline miss thì behaviour gì?

**V10.** Demo-time scaling có apply cho tất cả timers hay chỉ traffic-light timers?

---

# W. PoC Scope & HD Target

Project yêu cầu complete design, nhưng minimum Pass implementation chỉ cần 1/3 map: I1–I2 + railway crossing; higher complexity chỉ có giá trị nếu sensible và demonstrable. 

**W1.** Full **design** của team sẽ cover toàn bộ I1–I6 + RC1–RC3 chứ?

**W2.** Full **implementation** muốn cover bao nhiêu?

**W3.** Có thật sự implement 9 local controllers không?

**W4.** Hay implement representative `I1 + RC1 + I2 + C1` rồi demonstrate scalable design?

**W5.** HD features nào team chắc chắn muốn claim?

**W6.** Advanced sequence update có implement thật không?

**W7.** Congestion control có implement thật không?

**W8.** Railway fault handling có implement thật không?

**W9.** Pedestrian requests có implement thật không?

**W10.** Communication failure/recovery có demonstrate thật không?

**W11.** Central override có demonstrate thật không?

**W12.** Multiple QNX nodes deployment cuối cùng như nào?

Quan trọng nhất: **m trả lời gì tao sẽ coi đó là design candidate, không tự động biến nó thành final assumption**. Tao sẽ check mỗi decision theo ba trạng thái: `requirement từ đề / team assumption / design decision`, chỉ ra conflict hoặc consequence trước khi freeze. Như vậy mình sẽ không lặp lại chuyện assumption table bị lẫn requirement nữa. 
