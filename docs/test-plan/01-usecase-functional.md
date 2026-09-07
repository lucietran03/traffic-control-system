# Test Plan — Functional Test Case theo Use Case (UC-01 .. UC-10)

Tài liệu này liệt kê test case chức năng cho từng use case mô tả trong
`usecase.md` (mục 3.2), đối chiếu trực tiếp với code hiện có trong `app/`
(nhánh `main`, commit `b8c1ac8`). Mọi bước thao tác (phím bấm, lệnh
operator, prompt nhập số) được lấy nguyên văn từ:

- `app/intersection/src/lx_sensor.c` — bàn phím giả lập cảm biến Lx.
- `app/railway/src/rlx_sensor.c` — bàn phím giả lập cảm biến RLx.
- `app/central/src/c_operator.c` — console lệnh operator của C1.
- `app/central/src/c_hmi.c`, `app/central/src/c_logger.c`,
  `app/central/src/c_comm.c` — định dạng log/bảng trạng thái dùng để xác
  nhận kết quả mong đợi.
- `app/intersection/includes/lx_timer.h`, `app/railway/includes/rlx_timer.h`,
  `app/railway/includes/rlx_gate.h`, `app/shared/includes/sys_types.h`,
  `app/shared/includes/ipc_msg.h` — hằng số thời gian và mã số enum chính
  xác dùng trong "Kết quả mong đợi".

Nhóm có máy QNX thật, nên các test case đòi hỏi nhiều tiến trình/nhiều
node đều được viết đầy đủ, không né tránh — kể cả những test case cần chờ
thật (ví dụ chu kỳ đèn 48s/30s, hay chu trình đường sắt ~50s).

## 1. Quy ước ba môi trường chạy test

| Ký hiệu | Ý nghĩa | Ví dụ lệnh |
|---|---|---|
| **(A)** | 1 node QNX đơn — chỉ chạy 1 binary, không cần binary nào khác đang chạy. | `/tmp/lx_main 1` (chỉ L1, không có C1/RLx nào khác) |
| **(B)** | Nhiều node trên **cùng 1 máy** QNX — chạy nhiều binary cùng lúc trên cùng target, Qnet same-node (`name_open()` mặc định resolve nội bộ). **Không cần set** `TRAFFIC_NODE_MAP`. | mở nhiều shell trên cùng 1 target: `/tmp/c_main`, `/tmp/lx_main 1`, `/tmp/rlx_main 1` |
| **(C)** | Nhiều node trên **nhiều máy/VM** QNX thật qua mạng Qnet — bắt buộc export `TRAFFIC_NODE_MAP` trên mỗi shell trước khi chạy binary tương ứng. Xem `app/shared/README.md` mục "Cross-node resolution" và `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` mục 2.2. | `export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,...,rl1=VM_x86_Target03,..."` rồi chạy binary trên VM tương ứng |

Mọi test case dưới đây ghi rõ môi trường (A)/(B)/(C) cần dùng. Phần lớn
dùng (A)/(B) vì đã đủ để quan sát hành vi cần kiểm; (C) chỉ dùng khi bản
chất test là về mất kết nối Qnet giữa các máy vật lý khác nhau — với (B)
"mất kết nối" được mô phỏng bằng cách **không khởi động** hoặc **kill**
tiến trình liên quan, hiệu ứng quan sát được (heartbeat mất, `name_open()`
thất bại) giống hệt (C).

Thứ tự khởi động khuyến nghị theo `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` mục
2.3: **C1 trước → RLx → Lx sau cùng**. Binary build bằng `make` nằm ở
`build/bin/{c_main,lx_main,rlx_main}`; trên target đã deploy thì nằm ở
`/tmp/{c_main,lx_main,rlx_main}` — các bước dưới đây dùng đường dẫn
`/tmp/...` nhưng có thể thay bằng `build/bin/...` nếu chạy tại chỗ.

## 2. Cách đọc một test case

```
### TC-UCxx-y: <tên ngắn>
- **Loại**: Positive / Negative / Edge case
- **Liên quan**: UC-xx <tên>, bước nào trong main/alt flow (theo usecase.md)
- **Môi trường**: (A)/(B)/(C) — xem mục 1
- **Chuẩn bị**: trạng thái ban đầu, node nào cần chạy trước, ở đâu
- **Các bước**: phím bấm / lệnh operator / prompt nhập số theo đúng thứ tự
- **Kết quả mong đợi**: dòng log / trạng thái chính xác cần quan sát
```

Quy ước phím tắt (để đối chiếu nhanh khi đọc "Các bước", lấy nguyên văn từ
code — không tự bịa thêm phím):

- **Lx sensor** (`lx_sensor.c`, chạy trong tiến trình `lx_main`): `a`/`A` =
  xe đến/rời làn trục chính (arterial), `c`/`C` = xe đến/rời làn nhánh
  (connector), `1`/`2`/`3`/`4` = nút bấm người đi bộ phía 0/1/2/3, `w`/`W`
  = cảnh báo hàng chờ (queue warning) bật/tắt, `h`/`?` = help, `q` = dừng
  luồng đọc bàn phím (không dừng tiến trình).
- **RLx sensor** (`rlx_sensor.c`, chạy trong tiến trình `rlx_main`): `0`/`1`
  = tàu đang đến hướng 0/1 (`TRAIN_APPROACHING`), `x` = arm lỗi xác nhận
  chắn cho lần đóng/mở kế tiếp (demo RC-06), `f` = trigger fault-clear cục
  bộ (demo only, không đi qua đường dây `MSG_REQUEST_FAULT_CLEAR` thật),
  `h`/`?` = help, `q` = dừng luồng đọc bàn phím.
- **C1 operator console** (`c_operator.c`, chạy trong tiến trình `c_main`):
  `m` = `SET_MODE`, `t` = broadcast `SET_TIMING_PROFILE` cho R1/R2, `o` =
  `REQUEST_OVERRIDE`, `r` = `RENEW_OVERRIDE`, `c` = `CANCEL_OVERRIDE`, `f` =
  `REQUEST_FAULT_CLEAR` (chỉ nhắm RLx), `h`/`?` = help, `q` = dừng console.
  Mỗi lệnh chữ cái sẽ in ra 1-2 prompt nhập số theo đúng thứ tự đã cài
  trong `handle_*()` tương ứng — được ghi chính xác trong từng test case.

Bảng vị trí kề (RLx nào đứng cạnh Lx nào), lấy từ `app/railway/src/rlx_comm.c`
(`ADJACENCY[]`), dùng trong các test UC-04/UC-05/UC-08/UC-10:

| RLx | Lx kề (2 phía) |
|---|---|
| RL1 | L1, L2 |
| RL2 | L3, L4 |
| RL3 | L5, L6 |

Bảng hằng số thời gian dùng xuyên suốt (không lặp lại giải thích ở từng
test case):

| Hằng số | Giá trị | Nguồn |
|---|---|---|
| `LX_PEAK_ARTERIAL_GREEN_MS` | 48000 ms | `lx_timer.h` |
| `LX_PEAK_CONNECTOR_GREEN_MS` | 30000 ms | `lx_timer.h` |
| `LX_YELLOW_MS` | 4000 ms | `lx_timer.h` |
| `LX_ALL_RED_MS` | 2000 ms | `lx_timer.h` |
| `LX_MIN_GREEN_MS` / `LX_MAX_GREEN_MS` | 8000 / 40000 ms | `lx_timer.h` |
| `LX_EXTENSION_MS` | 4000 ms | `lx_timer.h` |
| `LX_WALK_MS` / `LX_FLASHING_DONT_WALK_MS` | 6000 / 4000 ms | `lx_timer.h` |
| `LX_DRAIN_MAX_EXTENSION_MS` | 60000 ms | `lx_timer.h` |
| `LX_OVERRIDE_DURATION_CAP_MS` | 300000 ms | `lx_timer.h` |
| `LX_CYCLE_LENGTH_MS` | 90000 ms (= 48+4+2+30+4+2 giây) | `lx_timer.h` |
| `RLX_WARNING_TO_CLOSING_MS` | 5000 ms | `rlx_timer.h` |
| `RLX_CLOSING_DEADLINE_MS` / `RLX_OPENING_DEADLINE_MS` | 15000 ms | `rlx_timer.h` |
| `RLX_EXPECTED_ARRIVAL_MS` | 20000 ms | `rlx_timer.h` |
| `RLX_OCCUPANCY_WINDOW_MS` | 20000 ms | `rlx_timer.h` |
| `RLX_GATE_MOTION_MS` | 3000 ms | `rlx_gate.h` |
| heartbeat Lx/RLx → C1 | mỗi 1000 ms | `lx_main.c`/`rlx_main.c` |
| ngưỡng UNAVAILABLE (PA-07) | 3 tick liên tiếp không có báo cáo (~3s) | `c_watchdog_mon.c` |

`controller_id_t` số nguyên xuất hiện trong log (`sys_types.h`): C1=0,
L1..L6=1..6, RL1..RL3=7..9. `msg_result_t`: ACK=1, ACK_PENDING=2, NACK=3,
ERROR=4. `supervisory_state_t` (cột SUPERVISORY trong bảng HMI của C1):
FAULT_SAFE=0, RAILWAY_PREEMPTION=1, CENTRAL_OVERRIDE=2, NORMAL_OPERATION=3.

---

## UC-01 — Serve Vehicle Demand

### TC-UC01-1: Chu kỳ PEAK_FIXED mặc định chạy đúng lịch cố định
- **Loại**: Positive
- **Liên quan**: UC-01 main flow bước 1-5, alt 2.1 (Peak Fixed operation)
- **Môi trường**: (A) chỉ `lx_main 1`
- **Chuẩn bị**: không có gì đặc biệt — L1 khởi động ở `MODE_PEAK_FIXED`, pha
  bắt đầu là `PHASE_ARTERIAL_GREEN` (mặc định trong `lx_fsm_init()`).
- **Các bước**:
  1. Chạy `/tmp/lx_main 1`.
  2. Không bấm phím nào, chỉ quan sát log trong ít nhất 90 giây (đúng 1 chu
     kỳ `LX_CYCLE_LENGTH_MS`).
- **Kết quả mong đợi**: log in đúng thứ tự và đúng mốc thời gian tương đối
  kể từ lúc start: `t=0` "Lx 1: SIGNAL -> ARTERIAL GREEN" (in ngay lúc
  `lx_fsm_init()`), `t=48s` "... -> ARTERIAL YELLOW", `t=52s` "... -> ALL
  RED (A to B)", `t=54s` "... -> CONNECTOR GREEN", `t=84s` "... ->
  CONNECTOR YELLOW", `t=88s` "... -> ALL RED (B to A)", `t=90s` quay lại
  "... -> ARTERIAL GREEN". Không có bất kỳ dòng nào khác xen giữa vì không
  có demand/ped/queue nào được bấm.

### TC-UC01-2: OFF_PEAK_SENSOR không có demand thì nghỉ ở pha arterial (alt 2.2)
- **Loại**: Positive
- **Liên quan**: UC-01 alt 2.2 "No Off-Peak demand is present"
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: cả hai tiến trình đang chạy, chưa bấm phím demand nào trên L1.
- **Các bước**:
  1. Trên `c_main`, bấm `m` → prompt "  Lx number (1-6): " nhập `1` → prompt
     "  mode (0=PEAK_FIXED, 1=OFF_PEAK_SENSOR): " nhập `1`.
  2. Chờ tối đa 54 giây (để L1 đi hết ARTERIAL_GREEN + YELLOW + ALL_RED hiện
     tại, vì mode mới chỉ áp dụng ở ranh giới ALL_RED tiếp theo — UC-07 BR-2).
  3. Sau khi L1 vào lại `PHASE_ARTERIAL_GREEN` dưới `MODE_OFF_PEAK_SENSOR`,
     không bấm phím `a`/`c`/`1`..`4` nào cả — chỉ quan sát tiếp 60 giây.
- **Kết quả mong đợi**: bước 1 in "C1: SET_MODE to 1 -> ACK_PENDING" (khác
  mode hiện tại nên `lx_fsm_on_set_mode()` trả `RESULT_ACK_PENDING`, xem
  `lx_fsm.c` dòng 660-676). Sau khi mode đổi, L1 **không bao giờ** tự thoát
  `ARTERIAL_GREEN` (không có dòng "SIGNAL -> ARTERIAL YELLOW" nào xuất hiện
  trong 60 giây quan sát) — vì `lx_fsm_on_phase_timer()` chỉ kiểm tra thoát
  pha ở đúng mốc 4s (`green_elapsed_ms % 4000 == 0`) và điều kiện thoát cần
  `own_demand` hoặc `maxed`; không có demand nào thì không thoát (nghỉ vô
  hạn ở arterial theo DP-04/BR-3).

### TC-UC01-3: Edge — thoát green đúng ngưỡng tối thiểu 8000 ms khi không có demand riêng nhưng có demand đối diện
- **Loại**: Edge case
- **Liên quan**: UC-01 main flow bước 6, BR-2 ("Off-Peak green phải từ 8-40s")
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 đã ở `MODE_OFF_PEAK_SENSOR` (làm như TC-UC01-2 bước 1-2),
  đang nghỉ ở `PHASE_ARTERIAL_GREEN` với `green_elapsed_ms` vừa reset về 0
  (ngay sau dòng "SIGNAL -> ARTERIAL GREEN" mới nhất).
- **Các bước**:
  1. Ngay khi vừa thấy dòng "SIGNAL -> ARTERIAL GREEN", bấm `c` trên `lx_sensor`
     của L1 (connector demand = 1). **Không** bấm `a` (arterial demand giữ
     nguyên 0).
  2. Bấm giờ, chờ đúng đến giây thứ 8 kể từ dòng ARTERIAL GREEN đó.
- **Kết quả mong đợi**: tại `t=8000ms` (đúng `LX_MIN_GREEN_MS`, mốc kiểm
  tra 4000ms-đầu-tiên-đủ-điều-kiện), L1 in "Lx 1: SIGNAL -> ARTERIAL
  YELLOW" ngay lập tức — không sớm hơn (mốc `t=4000ms` chưa đủ
  `LX_MIN_GREEN_MS` nên bị giữ lại) và không trễ hơn (vì
  `own_demand=arterial_vehicle_demand(0)||ped(0)=0` nên
  `lx_timer_should_exit_green()` trả về true ngay khi vừa qua min-green,
  không cần đợi tới 40000ms).

### TC-UC01-4: Edge — demand liên tục vẫn bị ép thoát đúng ngưỡng tối đa 40000 ms (chống bỏ đói connector)
- **Loại**: Edge case
- **Liên quan**: UC-01 alt 6.2 ("Connector demand is waiting"), BR-2, BR-4 (DP-06)
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: giống TC-UC01-3 nhưng lần này giữ demand arterial luôn bật.
- **Các bước**:
  1. Ngay khi thấy "SIGNAL -> ARTERIAL GREEN" (dưới `MODE_OFF_PEAK_SENSOR`),
     bấm `a` (arterial demand = 1) rồi bấm `c` (connector demand = 1).
     Không bấm `A`/`C` để clear trong suốt phase này.
  2. Chờ đến đúng giây thứ 40 kể từ dòng ARTERIAL GREEN đó.
- **Kết quả mong đợi**: mỗi mốc 4s (t=8000, 12000, ..., 36000) L1 **không**
  thoát pha (vì `own_demand=1` và chưa `maxed`), chỉ đến đúng
  `t=40000ms` (`LX_MAX_GREEN_MS`) mới in "Lx 1: SIGNAL -> ARTERIAL YELLOW"
  — vì `maxed = (elapsed >= 40000)` ép `!own_demand||maxed` thành true bất
  kể `arterial_vehicle_demand` vẫn còn 1, đúng theo BR-2/DP-06 (connector
  đã chờ sẵn không được bỏ đói).

### TC-UC01-5: Negative — connector demand bị treo an toàn khi railway pre-emption chiếm quyền (alt 3.1)
- **Loại**: Negative
- **Liên quan**: UC-01 alt 3.1 ("A higher-priority condition arises")
- **Môi trường**: (B) `c_main` + `lx_main 1` + `rlx_main 1` (RL1 kề L1)
- **Chuẩn bị**: cả 3 tiến trình chạy, L1 ở `MODE_PEAK_FIXED` mặc định.
- **Các bước**:
  1. Trên `lx_sensor` của L1, bấm `c` (connector demand = 1) — demand này
     chỉ có tác dụng ở `OFF_PEAK_SENSOR` nhưng vẫn được ghi nhận latch nội
     bộ; mục đích của test là chứng minh connector **không bao giờ** được
     phục vụ trong lúc bị chiếm quyền, bất kể demand có hay không.
  2. Trên `rlx_sensor` của RL1, bấm `0` (TRAIN_APPROACHING hướng 0).
  3. Quan sát log của L1 liên tục trong 60 giây kể từ lúc bấm `0`.
- **Kết quả mong đợi**: trong vòng ~1 giây sau bước 2, L1 nhận
  `MSG_CROSSING_STATUS(WARNING)` từ RL1 và chuyển
  `supervisory=SUPERVISORY_RAILWAY_PREEMPTION`; từ thời điểm đó cho đến khi
  RL1 báo lại `OPEN` (~52 giây sau, xem UC-04), log của L1 **chỉ** lặp lại
  "SIGNAL -> ARTERIAL GREEN" → "ARTERIAL YELLOW" → "ALL RED (A to B)" →
  "ARTERIAL GREEN" — **không bao giờ** xuất hiện "SIGNAL -> CONNECTOR
  GREEN" trong suốt cửa sổ này (vì tại ranh giới `PHASE_ALL_RED_A_TO_B`,
  nhánh `SUPERVISORY_RAILWAY_PREEMPTION` ép quay lại
  `PHASE_ARTERIAL_GREEN` — `lx_fsm.c` dòng 262-277). Demand connector vẫn
  còn nguyên (`connector_vehicle_demand=1`), chỉ được phục vụ an toàn sau
  khi preemption kết thúc.

---

## UC-02 — Serve Pedestrian Crossing Request

### TC-UC02-1: Trình tự WALK → FLASHING_DONT_WALK → DONT_WALK đúng thời lượng
- **Loại**: Positive
- **Liên quan**: UC-02 main flow bước 1-8, BR-1
- **Môi trường**: (A) chỉ `lx_main 1`
- **Chuẩn bị**: L1 vừa khởi động, đang ở `PHASE_ARTERIAL_GREEN` (side 0/1
  tương thích với arterial green theo `lx_fsm.c` dòng 136-138).
- **Các bước**:
  1. Ngay sau dòng "SIGNAL -> ARTERIAL GREEN" đầu tiên, bấm `1` (nút người
     đi bộ phía 0).
  2. Quan sát log trong 11 giây tiếp theo.
- **Kết quả mong đợi**: trong vòng 100ms sau khi bấm, in "Lx 1: PED SIGNAL
  side 0 -> WALK"; đúng `t=6000ms` sau đó in "... side 0 ->
  FLASHING_DONT_WALK"; đúng `t=10000ms` (6000+4000) in "... side 0 ->
  DONT_WALK". Sau dòng DONT_WALK, `ped_latched[0]` được xoá — không có
  WALK nào lặp lại nếu không bấm `1` thêm lần nữa.

### TC-UC02-2: Negative/alt 2.1 — bấm nút lặp lại trước khi được phục vụ chỉ tạo đúng 1 yêu cầu
- **Loại**: Negative
- **Liên quan**: UC-02 alt 2.1 ("A request is already pending"), BR-3
- **Môi trường**: (A) chỉ `lx_main 1`
- **Chuẩn bị**: L1 vừa khởi động, ở `PHASE_ARTERIAL_GREEN`.
- **Các bước**:
  1. Bấm `1` ba lần liên tiếp thật nhanh (trong cùng 1 tick 100ms, trước
     khi bất kỳ dòng WALK nào kịp in ra).
  2. Quan sát 11 giây tiếp theo.
- **Kết quả mong đợi**: chỉ có **đúng một** chuỗi WALK→FDW→DONT_WALK cho
  side 0 xuất hiện (không có 3 chuỗi song song/nối tiếp) — vì
  `lx_fsm_latch_pedestrian_request()` chỉ set `ped_latched[0]=1` (idempotent,
  `lx_fsm.c` dòng 425-438), không đếm số lần bấm.

### TC-UC02-3: Negative/alt 3.1 tương đương — yêu cầu bị giữ (không mất) khi phía tương thích bị railway pre-emption chặn
- **Loại**: Negative
- **Liên quan**: UC-02 alt 3.1 ("Railway restriction prevents immediate service"), BR-4 (PA-02)
- **Môi trường**: (B) `lx_main 1` + `rlx_main 1` (RL1 kề L1)
- **Chuẩn bị**: L1 ở `MODE_PEAK_FIXED`, đang ở pha bất kỳ. Side 2/3 chỉ
  tương thích với `PHASE_CONNECTOR_GREEN` (`lx_fsm.c` dòng 139-141), và
  `CONNECTOR_GREEN` bị chặn hoàn toàn trong lúc `RAILWAY_PREEMPTION`
  (giống TC-UC01-5).
- **Các bước**:
  1. Trên `rlx_sensor` của RL1, bấm `0` để bắt đầu pre-emption.
  2. Ngay sau đó (trong vòng vài giây), trên `lx_sensor` của L1 bấm `3`
     (nút người đi bộ phía 2).
  3. Quan sát log của L1 liên tục cho đến khi RL1 báo `OPEN` trở lại
     (~52 giây sau bước 1, xem UC-04) và ít nhất 10 giây sau đó.
- **Kết quả mong đợi**: trong suốt thời gian pre-emption, **không** xuất
  hiện dòng "PED SIGNAL side 2 -> WALK" nào (vì `CONNECTOR_GREEN` không
  bao giờ được vào). Yêu cầu vẫn được giữ nguyên (`ped_latched[2]` không
  bị xoá). Chỉ sau khi RL1 báo `OPEN`, `supervisory` của L1 trở lại
  `NORMAL_OPERATION`, và ở lần `CONNECTOR_GREEN` đầu tiên sau đó, "Lx 1:
  PED SIGNAL side 2 -> WALK" mới xuất hiện, đúng theo PA-02 "không được
  huỷ yêu cầu nhận trong lúc pre-emption".

### TC-UC02-4: Edge — bấm lại đúng lúc đang FLASHING_DONT_WALK không bị mất yêu cầu (bug fix)
- **Loại**: Edge case
- **Liên quan**: UC-02 main flow bước 5 kết hợp bước 2 (regression test cho fix `ped_recall`)
- **Môi trường**: (A) chỉ `lx_main 1`
- **Chuẩn bị**: L1 vừa khởi động, ở `PHASE_ARTERIAL_GREEN`.
- **Các bước**:
  1. Ngay sau "SIGNAL -> ARTERIAL GREEN", bấm `1` (side 0) → WALK bắt đầu.
  2. Đợi đúng 8 giây kể từ lúc bấm (tức đang ở giữa cửa sổ
     FLASHING_DONT_WALK, vì WALK kéo dài 6000ms rồi FDW bắt đầu, 8s là
     2s sau khi FDW bắt đầu, còn 2s nữa mới hết FDW).
  3. Bấm `1` lần nữa (side 0) ngay tại giây thứ 8 này.
  4. Tiếp tục quan sát đến giây thứ 12 (10000ms hết FDW + dư).
- **Kết quả mong đợi**: tại bước 3, vì `ped_serving_mask` đang chứa side 0
  (đang phục vụ), `lx_fsm_latch_pedestrian_request()` set `ped_recall[0]=1`
  thay vì tạo yêu cầu mới ngay (`lx_fsm.c` dòng 429-434). Tại `t=10000ms`,
  log vẫn in "... side 0 -> DONT_WALK" như bình thường, **nhưng vì
  `ped_recall[0]==1`, `ped_latched[0]` KHÔNG bị xoá** (`lx_fsm.c` dòng
  177-186) — ngay tick kế tiếp (t=10100ms), một chuỗi WALK **mới** cho
  side 0 lập tức bắt đầu lại: "Lx 1: PED SIGNAL side 0 -> WALK" xuất hiện
  lần thứ hai mà không cần bấm `1` thêm lần nào. Đây chính là hành vi được
  sửa (trước fix, yêu cầu bấm giữa chừng sẽ bị mất do `ped_latched[0]`
  không được set lại vì đã là 1 sẵn).

---

## UC-03 — Coordinate Arterial Traffic Progression

### TC-UC03-1: Positive — broadcast timing profile cho chuỗi R1 (L1/L3/L5) được cả 3 controller ACK
- **Loại**: Positive
- **Liên quan**: UC-03 main flow bước 1-8, BR-1, BR-2
- **Môi trường**: (B) `c_main` + `lx_main 1` + `lx_main 3` + `lx_main 5`
- **Chuẩn bị**: cả 4 tiến trình đang chạy, chưa gửi timing profile nào
  (`next_profile_id` đang là 1).
- **Các bước**:
  1. Trên `c_main`, bấm `t` → prompt "  chain (1=R1 L1/L3/L5, 2=R2
     L2/L4/L6): " nhập `1`.
- **Kết quả mong đợi**: log C1 in "Operator: SET_TIMING_PROFILE broadcast
  (chain=R1, profile_id=1) submitted", sau đó 3 dòng riêng biệt "C1:
  SET_TIMING_PROFILE to 1 -> ACK", "... to 3 -> ACK", "... to 5 -> ACK"
  (offset gửi tương ứng L1=0ms, L3=21000ms, L5=45000ms — đều < 90000ms nên
  luôn hợp lệ, xem `c_mode_eng.h`). Vì offset chỉ áp dụng tại lần vào
  `PHASE_ARTERIAL_GREEN` tươi kế tiếp (`lx_fsm.c` dòng 339-345,
  `offset_apply_pending`), L1/L3/L5 không đổi ngay pha hiện tại — chờ đến
  khi mỗi controller tự nhiên vào lại ARTERIAL_GREEN (tối đa 90 giây sau)
  để thấy `green_elapsed_ms` của chúng được nắn theo offset đã cấp.

### TC-UC03-2: Negative — số hiệu chain không hợp lệ bị huỷ ngay tại console, không gửi đi
- **Loại**: Negative
- **Liên quan**: UC-03 — vùng nhập liệu của Trigger ("operator submits a validated ... profile")
- **Môi trường**: (B) `c_main` + `lx_main 1` (không bắt buộc nhưng để xác
  nhận không nhận được gì)
- **Chuẩn bị**: c_main đang chạy.
- **Các bước**:
  1. Bấm `t` → prompt chain, nhập `3` (không phải 1 hoặc 2).
- **Kết quả mong đợi**: in ngay "c_operator: 3 is not a valid chain (1 or
  2) - command aborted"; **không** có dòng "Operator: SET_TIMING_PROFILE
  broadcast..." nào được log, và L1 không nhận được `MSG_SET_TIMING_PROFILE`
  nào (không có "SIGNAL ->" nào bất thường, không có ACK nào trong log C1).

### TC-UC03-3: Negative/alt 2.1 — L2 chưa từng nhận profile vẫn tự vận hành standalone bình thường
- **Loại**: Negative
- **Liên quan**: UC-03 alt 2.1 ("The coordination profile is missing or stale")
- **Môi trường**: (A) chỉ `lx_main 2`
- **Chuẩn bị**: chỉ chạy L2, không có C1, không gửi bất kỳ
  `SET_TIMING_PROFILE` nào.
- **Các bước**:
  1. Chạy `/tmp/lx_main 2`, không thao tác gì thêm.
  2. Quan sát 90 giây.
- **Kết quả mong đợi**: L2 chạy đúng chu kỳ `PEAK_FIXED` mặc định giống hệt
  TC-UC01-1 (48s/4s/2s/30s/4s/2s), không "chờ" hay bị treo ở bất kỳ đâu để
  đợi Central — vì `assigned_offset_ms`/`offset_apply_pending` mặc định là
  0 (`lx_fsm_init()`), tương đương "không có offset" chứ không phải lỗi.

### TC-UC03-4: Edge — gửi `t` hai lần liên tiếp cho cùng chuỗi, profile_id tăng dần và không phá vỡ lần trước
- **Loại**: Edge case
- **Liên quan**: UC-03 main flow bước 2 (nguồn phát `profile_id`)
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: `next_profile_id` đang ở giá trị bất kỳ N (ví dụ N=1 nếu
  test độc lập, không chạy sau TC-UC03-1 trên cùng tiến trình c_main).
- **Các bước**:
  1. Bấm `t` → chain `1`. Ghi lại `profile_id` trong log (ví dụ N).
  2. Ngay lập tức bấm `t` → chain `1` lần nữa.
- **Kết quả mong đợi**: dòng log thứ hai hiển thị `profile_id=N+1` (đúng
  theo `c_mode_eng_next_profile_id()` — hậu tăng, `c_mode_eng.c` dòng
  81-84), và L1 nhận **cả hai** `MSG_SET_TIMING_PROFILE` liên tiếp, cả hai
  đều `ACK` (offset không đổi nên `payload->offset_ms(0) < LX_CYCLE_LENGTH_MS`
  luôn true) — `active_profile_id`/`assigned_offset_ms` của L1 cuối cùng
  phản ánh lần gửi thứ hai (N+1), không bị kẹt ở trạng thái trung gian.

---

## UC-04 — Protect a Railway Crossing for an Approaching Train

### TC-UC04-1: Positive — trình tự đầy đủ WARNING → CLOSING → CLOSED → TRAIN_PRESENT → OPENING → OPEN
- **Loại**: Positive
- **Liên quan**: UC-04 main flow bước 1-11, BR-1, BR-2, BR-3
- **Môi trường**: (B) `rlx_main 1` + `lx_main 1` + `lx_main 2` (RL1 kề L1, L2)
- **Chuẩn bị**: cả 3 tiến trình chạy, crossing đang `OPEN`.
- **Các bước**:
  1. Trên `rlx_sensor` của RL1, bấm `0` (TRAIN_APPROACHING hướng 0) tại
     mốc `t=0`.
  2. Không thao tác gì thêm, quan sát log RL1 và log L1/L2 liên tục trong
     55 giây.
- **Kết quả mong đợi** (mốc thời gian tính từ `t=0`):
  - `t=0`: RL1 in "RLx: flashers ON (train approaching, direction 0)".
  - `t≈0-1s`: L1 và L2 mỗi bên nhận `MSG_CROSSING_STATUS(WARNING)` (broadcast
    ở tick 1s kế tiếp) — không có log riêng ở phía Lx cho sự kiện này,
    nhưng từ lúc này `supervisory` của cả L1 và L2 chuyển
    `RAILWAY_PREEMPTION`.
  - `t=5s` (`RLX_WARNING_TO_CLOSING_MS`): RL1 in "RLx: commanding gates
    DOWN (simulated motion, 3000 ms)".
  - `t=8s` (5000+`RLX_GATE_MOTION_MS`): gate xác nhận đóng, RL1 in "RLx:
    train signal PROCEED for direction 0 (gates confirmed closed)".
  - Từ `t≈0` đến `t≈52s`: log của L1/L2 **không** có dòng "SIGNAL ->
    CONNECTOR GREEN" nào (giống TC-UC01-5).
  - `t=28s` (8000+`RLX_EXPECTED_ARRIVAL_MS`=8000+20000): crossing chuyển
    `TRAIN_PRESENT` nội bộ (không có log riêng, chỉ suy ra qua occupancy
    window bắt đầu đếm 20000ms).
  - `t=48s` (28000+20000): active_window_count về 0, RL1 in "RLx: all
    train signals -> STOP (crossing reopening)" rồi "RLx: commanding gates
    UP (simulated motion, 3000 ms)".
  - `t=51s` (48000+3000): RL1 in "RLx: flashers OFF (gates confirmed
    open)".
  - `t≈51-52s`: L1/L2 nhận `MSG_CROSSING_STATUS(OPEN)`, `supervisory` trở
    lại `NORMAL_OPERATION`.

### TC-UC04-2: Negative/alt 6.1 — gate không xác nhận đóng dẫn tới FAULT, tín hiệu tàu giữ STOP
- **Loại**: Negative
- **Liên quan**: UC-04 alt 6.1 ("Gate closure is not confirmed"), BR-2
- **Môi trường**: (B) `rlx_main 1` + `lx_main 1` + `lx_main 2`
- **Chuẩn bị**: crossing `OPEN`.
- **Các bước**:
  1. Trên `rlx_sensor` của RL1, bấm `x` (arm lỗi xác nhận cho lần đóng kế
     tiếp).
  2. Ngay sau đó bấm `0` (TRAIN_APPROACHING hướng 0) tại `t=0`.
  3. Quan sát 21 giây.
- **Kết quả mong đợi**:
  - `t=5s`: "RLx: commanding gates DOWN (simulated motion, 3000 ms)".
  - `t=8s`: gate motion "hoàn tất" nhưng bị armed fail — RL1 in "RLx: gate
    FAILED TO CONFIRM (simulated fault) - rlx_fsm.c's own deadline will
    raise FAULT_GATE_CONFIRM_MISSING". **Không** có dòng "train signal
    PROCEED" nào xuất hiện.
  - `t=20s` (5000+`RLX_CLOSING_DEADLINE_MS`=5000+15000): RL1 in "RLx: FAULT
    latched (fault bit 0x1) - holding STOP on all train signals,
    commanding gates DOWN".
  - Trong suốt và sau `t=20s`, tín hiệu tàu không bao giờ hiển thị
    PROCEED; L1/L2 vẫn giữ `RAILWAY_PREEMPTION` (vì crossing state báo
    `FAULT` ≠ `OPEN`), tức connector vẫn bị suy suppressed vô thời hạn cho
    đến khi fault được xử lý (xem UC-06).

### TC-UC04-3: Edge/alt 8.1 — tàu thứ hai đến trong lúc TRAIN_PRESENT kéo dài thời gian đóng chắn
- **Loại**: Edge case
- **Liên quan**: UC-04 alt 8.1 ("A second train is detected before reopening"), BR-3
- **Môi trường**: (B) `rlx_main 1` (không bắt buộc Lx cho test này, có thể
  thêm `lx_main 1`/`lx_main 2` nếu muốn quan sát cả 2 phía)
- **Chuẩn bị**: crossing `OPEN`.
- **Các bước**:
  1. Bấm `0` tại `t=0` (như TC-UC04-1, không arm fault).
  2. Tại đúng `t=35s` (13 giây sau khi vào TRAIN_PRESENT ở `t=28s`, cửa sổ
     hướng 0 còn 13s nữa mới hết ở `t=48s`), bấm `1` (TRAIN_APPROACHING
     hướng 1).
  3. Quan sát đến `t=60s`.
- **Kết quả mong đợi**: bước 2 đăng ký một occupancy window mới cho hướng 1
  với `remaining_ms=20000` ngay lập tức (vì đang ở `RLX_TRAIN_PRESENT`,
  `rlx_fsm_simulate_train_approaching()` case `RLX_TRAIN_PRESENT`, không
  cần đợi "expected arrival"). Cửa sổ hướng 0 hết ở `t=48s` nhưng
  `active_window_count` **không về 0** (còn cửa sổ hướng 1), nên RL1
  **không** in "gates UP" tại `t=48s`. Chỉ đến `t=55s` (35000+20000), khi
  cửa sổ hướng 1 cũng hết, `active_window_count` mới về 0 và RL1 mới in
  "RLx: all train signals -> STOP (crossing reopening)" rồi mở chắn — trễ
  hơn kịch bản 1 tàu đúng 7 giây, đúng theo RC-04 "gates remain closed
  while either occupancy window remains active".

### TC-UC04-4: Negative — tàu tiếp cận lặp lại trong cùng bước WARNING không làm reset lại đồng hồ 5 giây
- **Loại**: Negative
- **Liên quan**: UC-04 main flow bước 2 (self-loop trong `rlx_fsm_simulate_train_approaching()`), kiểm tra không có lỗi kéo dài WARNING ngoài ý muốn
- **Môi trường**: (A) chỉ `rlx_main 1`
- **Chuẩn bị**: crossing `OPEN`.
- **Các bước**:
  1. Bấm `0` tại `t=0`.
  2. Tại `t=2s`, bấm `1` (hướng khác, vẫn trong lúc còn `WARNING`).
  3. Quan sát đến `t=6s`.
- **Kết quả mong đợi**: bước 2 chỉ đăng ký thêm 1 occupancy window cho
  hướng 1 (self-loop, `rlx_fsm.c` dòng 247-253), **không** in thêm dòng
  "flashers ON" nào và **không** reset `state_elapsed_ms` — chắn vẫn
  chuyển sang CLOSING đúng tại `t=5s` (tính từ lần bấm `0` đầu tiên, không
  phải `t=7s` nếu đồng hồ bị reset sai).

---

## UC-05 — Manage Road Traffic During and After a Railway Closure

### TC-UC05-1: Positive — drain phase mở rộng theo bước 4 giây rồi kết thúc an toàn khi hết cảnh báo hàng chờ
- **Loại**: Positive
- **Liên quan**: UC-05 main flow bước 6-10, BR-3 (CC-03)
- **Môi trường**: (B) `rlx_main 1` + `lx_main 1`
- **Chuẩn bị**: L1 ở `MODE_PEAK_FIXED`. Kịch bản dùng lại toàn bộ chu trình
  đường sắt của TC-UC04-1 (~52 giây từ lúc bấm tàu đến khi crossing `OPEN`).
- **Các bước**:
  1. Bấm `0` trên RL1 tại `t=0` để bắt đầu pre-emption (như TC-UC04-1).
  2. Trong lúc còn đang `RAILWAY_PREEMPTION` (ví dụ tại `t=30s`), trên
     `lx_sensor` của L1 bấm `w` (queue warning = 1).
  3. Chờ đến khi crossing báo `OPEN` (~`t=52s`) — lúc này
     `drain_pending=1` được arm vì `queue_warning_active` đang bật đúng
     thời điểm chuyển `RAILWAY_PREEMPTION → NORMAL_OPERATION` (`lx_fsm.c`
     dòng 821-823).
  4. L1 tiếp tục chạy arterial hiện tại cho hết vòng của nó (tối đa 48s +
     4s + 2s nữa nếu vừa mới vào arterial), rồi tự nhiên vào
     `CONNECTOR_GREEN` — đây chính là drain phase (arm ở
     `lx_fsm_advance_phase_locked()` dòng 287-292).
  5. Khi `CONNECTOR_GREEN` này chạm mốc 30000ms (`LX_PEAK_CONNECTOR_GREEN_MS`)
     mà `queue_warning_active` vẫn còn bật, nó bắt đầu "extending" (không
     in log riêng, chỉ suy ra qua việc không chuyển YELLOW đúng 30s).
  6. Đúng 8 giây sau mốc 30s đó (2 lần gia hạn 4s), bấm `W` (queue warning
     = 0) trên L1.
- **Kết quả mong đợi**: L1 vẫn ở `CONNECTOR_GREEN` cho đến hết 30s +
  8s = 38s kể từ lúc vào connector green, sau đó — tại checkpoint 4s tiếp
  theo mà phát hiện `!queue_warning_active` — L1 in "Lx 1: SIGNAL ->
  CONNECTOR YELLOW" và kết thúc drain đúng an toàn (không vượt quá
  60000ms cap). Tổng thời gian connector green trong lần drain này xấp xỉ
  36-40 giây, dài hơn 30s bình thường đúng bằng bội số 4s.

### TC-UC05-2: Negative/alt 6.1 — không có cảnh báo hàng chờ thì bỏ qua drain hoàn toàn
- **Loại**: Negative
- **Liên quan**: UC-05 alt 6.1 ("No queue warning exists after reopening")
- **Môi trường**: (B) `rlx_main 1` + `lx_main 1`
- **Chuẩn bị**: giống TC-UC05-1 nhưng **không** bấm `w` bao giờ.
- **Các bước**:
  1. Bấm `0` trên RL1 tại `t=0`, không bấm `w`/`W` gì trên L1 trong suốt
     quá trình.
  2. Sau khi crossing `OPEN` (~`t=52s`), chờ L1 vào `CONNECTOR_GREEN` tiếp
     theo và quan sát đúng 31 giây kể từ lúc đó.
- **Kết quả mong đợi**: vì `queue_warning_active=0` tại thời điểm
  `RAILWAY_PREEMPTION → NORMAL_OPERATION`, `drain_pending` không bao giờ
  được set. `CONNECTOR_GREEN` kết thúc đúng tại mốc 30000ms như một chu kỳ
  PEAK_FIXED bình thường — in "SIGNAL -> CONNECTOR YELLOW" ngay tại
  `t=30s` kể từ lúc vào connector green, không có bất kỳ gia hạn nào.

### TC-UC05-3: Edge — cảnh báo hàng chờ không bao giờ hết, drain bị cắt cứng đúng tại cap 60000 ms
- **Loại**: Edge case
- **Liên quan**: UC-05 alt 9.1 ("Queue warning remains active at 60 seconds"), BR-3
- **Môi trường**: (B) `rlx_main 1` + `lx_main 1`
- **Chuẩn bị**: giống TC-UC05-1 đến hết bước 4 (drain đã armed và đã vào
  `CONNECTOR_GREEN`).
- **Các bước**:
  1. Bấm `w` trước khi crossing mở lại (như TC-UC05-1 bước 2) và **không
     bao giờ** bấm `W` sau đó.
  2. Từ lúc L1 vào `CONNECTOR_GREEN` (drain phase), quan sát liên tục đến
     phút thứ (30+60)/60 ≈ 1.5 phút sau khi vào pha này.
- **Kết quả mong đợi**: drain tiếp tục gia hạn từng 4 giây một
  (`drain_extension_total_ms` cộng dồn) bất kể `queue_warning_active` vẫn
  là 1, cho đến khi `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS`
  (60000ms) — đúng tại mốc 30000+60000=90000ms kể từ lúc vào connector
  green, L1 in "SIGNAL -> CONNECTOR YELLOW" ngay lập tức dù `queue_warning_active`
  vẫn đang bật (`lx_fsm.c` dòng 1022 `drain_extension_total_ms >=
  LX_DRAIN_MAX_EXTENSION_MS` thắng thế so với điều kiện warning), đúng
  theo cap cứng 60 giây.

### TC-UC05-4: Negative/alt 4.1 — crossing vào FAULT thì hướng connector vẫn bị giữ đỏ vô thời hạn
- **Loại**: Negative
- **Liên quan**: UC-05 alt 4.1 ("Crossing enters FAULT"), BR-1 (CC-02)
- **Môi trường**: (B) `rlx_main 1` + `lx_main 1`
- **Chuẩn bị**: crossing `OPEN`.
- **Các bước**:
  1. Trên RL1, bấm `x` rồi `0` (kích hoạt kịch bản FAULT như TC-UC04-2),
     đợi đến `t=20s` để crossing vào `FAULT`.
  2. Tiếp tục quan sát log L1 thêm 60 giây nữa (không thao tác gì thêm).
- **Kết quả mong đợi**: vì crossing report state = `FAULT` (≠ `OPEN`), L1
  giữ nguyên `supervisory=RAILWAY_PREEMPTION` vô thời hạn — không có dòng
  "SIGNAL -> CONNECTOR GREEN" nào xuất hiện trong toàn bộ 60 giây quan sát
  thêm, trong khi "SIGNAL -> ARTERIAL GREEN/YELLOW/ALL RED..." vẫn tiếp
  tục lặp lại bình thường (cross-traffic không bị ảnh hưởng, đúng CC-02
  "compatible cross-traffic ... continue"). Việc phục hồi chỉ có thể xảy
  ra sau khi fault được xử lý ở phía RLx (xem UC-06) và crossing báo
  `OPEN` trở lại.

---

## UC-06 — Respond to a Railway Equipment Fault

### TC-UC06-1: Positive — fault cục bộ giữ STOP ngay lập tức và báo cáo độc lập lên Central
- **Loại**: Positive
- **Liên quan**: UC-06 main flow bước 1-6, BR-1, BR-2
- **Môi trường**: (B) `c_main` + `rlx_main 1`
- **Chuẩn bị**: crossing `OPEN`.
- **Các bước**:
  1. Trên `rlx_sensor` của RL1, bấm `x` rồi `0` tại `t=0`.
  2. Quan sát log RL1 và log C1 đến `t=21s`.
- **Kết quả mong đợi**: tại `t=20s`, RL1 in "RLx: FAULT latched (fault bit
  0x1) - holding STOP on all train signals, commanding gates DOWN" (như
  TC-UC04-2). Trong vòng 1 giây sau đó (tick `IPC_PULSE_RAILWAY_WARNING`
  kế tiếp gọi `rlx_comm_send_fault_report()`), C1 in dòng log:
  `FAULT_REPORT from 7: fault_code=0x00000001 severity=1 detail="RLx fault - see fault_code bitmask"`
  (7 = `CTRL_RL1`). Nếu chạy thêm `c_hmi` (tick 1Hz sẵn có), bảng trạng
  thái của C1 sẽ hiện dòng RL1 với cột `FAULTS=0x1` và `CROSSING_STATE=3`
  (`CROSSING_FAULT`).

### TC-UC06-2: Negative/alt 5.1 — an toàn cục bộ không phụ thuộc Central
- **Loại**: Negative
- **Liên quan**: UC-06 alt 5.1 ("Central communication is unavailable")
- **Môi trường**: (A) chỉ `rlx_main 1` (không chạy `c_main`)
- **Chuẩn bị**: crossing `OPEN`, không có C1 nào đang chạy.
- **Các bước**:
  1. Bấm `x` rồi `0` tại `t=0`, giống TC-UC06-1.
  2. Quan sát 21 giây.
- **Kết quả mong đợi**: đúng tại `t=20s`, RL1 vẫn in "RLx: FAULT latched
  ..." giống hệt TC-UC06-1 — không có độ trễ, không bị treo chờ C1 (vì
  `rlx_comm_send_fault_report()` chỉ gọi `ipc_client_post()`, không
  blocking). Do không có C1, `ipc_client_post()` gửi thất bại
  (`name_open("traffic/c1")` không tìm thấy) nhưng chỉ log lỗi phía RLx
  ("RLx: message to 0 failed to send" từ `on_reply_log_failure` trong
  `rlx_comm.c`) — không ảnh hưởng đến trạng thái an toàn cục bộ (gates đã
  đóng, tín hiệu đã STOP) mà không cần C1 xác nhận gì cả.

### TC-UC06-3: Negative/alt 7.2 — yêu cầu fault-clear khi lỗi vật lý chưa thật sự hết bị NACK
- **Loại**: Negative
- **Liên quan**: UC-06 alt 7.2 ("Fault-clear request is premature"), BR-4 (PA-09)
- **Môi trường**: (B) `c_main` + `rlx_main 1`
- **Chuẩn bị**: RL1 đã vào `FAULT` như TC-UC06-1 (`t=20s` sau khi bấm
  `x`+`0`).
- **Các bước**:
  1. Ngay sau khi thấy "FAULT latched" trên RL1, trên `rlx_sensor` của RL1
     bấm `f` (demo fault-clear cục bộ).
  2. Riêng biệt, trên `c_main` bấm `f` → prompt "  RLx number (1-3): "
     nhập `1`.
- **Kết quả mong đợi**: bước 1 in "[rlx_sensor] fault-clear result=3
  reason=4" (`RESULT_NACK`=3, `NACK_REASON_FAULT_ACTIVE`=4) — vì
  `rlx_fsm_on_fault_clear()` kiểm tra `gates_confirmed_open()` và thấy
  false (gate đang đóng dở/lỗi, không phải đang mở). Bước 2 (đường dây
  Central thật) log C1 in "C1: REQUEST_FAULT_CLEAR to 7 -> NACK
  reason=FAULT_ACTIVE". Crossing vẫn ở `FAULT`, train signal vẫn `STOP`.

### TC-UC06-4: Edge — phát hiện giới hạn: đường ACK của fault-clear (alt 7.1) hiện không thể tái hiện qua demo gate simulator
- **Loại**: Edge case
- **Liên quan**: UC-06 alt 7.1 ("Operator requests fault clearance after
  repair"), đối chiếu với `rlx_gate.c`
- **Môi trường**: (B) `c_main` + `rlx_main 1`
- **Chuẩn bị**: RL1 đã ở `FAULT` như TC-UC06-3.
- **Các bước**:
  1. Bấm `f` trên `rlx_sensor` của RL1 ba lần liên tiếp, cách nhau vài
     giây.
  2. Bấm `f` từ operator console của C1 (target RLx=1) thêm một lần nữa.
- **Kết quả mong đợi**: **cả bốn lần** đều trả về NACK/`FAULT_ACTIVE` giống
  TC-UC06-3, không lần nào ACK. Đây là hệ quả trực tiếp của cách
  `enter_fault()` trong `rlx_fsm.c` luôn gọi `rlx_gate_command_close()`
  (đặt lại `g_confirmed_open=0` trong `rlx_gate.c`), và **không có bất kỳ
  đường code nào khác** từng gọi `rlx_gate_command_open()` trong khi
  `state==RLX_FAULT` (chỉ `enter_opening()` gọi, và nó chỉ được gọi từ
  `RLX_TRAIN_PRESENT`, không phải từ `RLX_FAULT`). Do đó, với bộ giả lập
  gate hiện tại, **không có cách nào qua các phím/lệnh có sẵn để đưa
  `gates_confirmed_open()` về true trong khi đang `FAULT`** — nghĩa là
  nhánh ACK của alt 7.1 hiện không thể kiểm thử end-to-end được. Ghi nhận
  đây là một khoảng trống cần báo lại cho đội phát triển (không phải lỗi
  của test case), không phải hành vi sai của FSM so với đặc tả.

---

## UC-07 — Configure Traffic Operating Parameters

### TC-UC07-1: Positive — SET_MODE khác mode hiện tại được ACK_PENDING rồi áp dụng ở ranh giới an toàn
- **Loại**: Positive
- **Liên quan**: UC-07 main flow bước 1-6, BR-2
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 đang `MODE_PEAK_FIXED` (mặc định), đang giữa
  `PHASE_ARTERIAL_GREEN`.
- **Các bước**:
  1. Bấm `m` → prompt "  Lx number (1-6): " nhập `1` → prompt "  mode
     (0=PEAK_FIXED, 1=OFF_PEAK_SENSOR): " nhập `1`.
- **Kết quả mong đợi**: C1 log "Operator: SET_MODE(target=1, mode=OFF_PEAK_SENSOR)
  submitted" rồi "C1: SET_MODE to 1 -> ACK_PENDING" (khác mode hiện tại →
  nhánh `else` của `lx_fsm_on_set_mode()`, `lx_fsm.c` dòng 670-676). L1
  **không** đổi hành vi ngay (vẫn chạy hết 48s arterial green hiện tại như
  PEAK_FIXED) — chỉ tại ranh giới `ALL_RED` kế tiếp (`lx_fsm.c` dòng
  239-242 hoặc 306-309), `fsm->mode` mới thực sự đổi thành
  `OFF_PEAK_SENSOR`.

### TC-UC07-2: Negative — số hiệu Lx ngoài phạm vi bị huỷ ngay tại console
- **Loại**: Negative
- **Liên quan**: UC-07 alt 3.1 tương đương (request không hợp lệ bị từ chối trước khi tới controller)
- **Môi trường**: (B) `c_main` (không cần Lx nào chạy)
- **Chuẩn bị**: c_main đang chạy.
- **Các bước**:
  1. Bấm `m` → prompt Lx number, nhập `7`.
- **Kết quả mong đợi**: in ngay "c_operator: 7 is not a valid Lx (1-6) -
  command aborted"; hàm trả về **trước** khi hỏi prompt mode — không có
  dòng "Operator: SET_MODE..." nào được log, không gửi gì đi.

### TC-UC07-3: Negative — giá trị mode không hợp lệ bị huỷ, không đụng tới bookkeeping
- **Loại**: Negative
- **Liên quan**: UC-07 BR-1 ("Normal mode selection uses only PEAK_FIXED and OFF_PEAK_SENSOR")
- **Môi trường**: (B) `c_main` + `lx_main 2`
- **Chuẩn bị**: c_main và L2 đang chạy.
- **Các bước**:
  1. Bấm `m` → Lx number `2` → mode nhập `2` (không phải 0 hoặc 1).
- **Kết quả mong đợi**: in "c_operator: 2 is not a valid mode (0 or 1) -
  command aborted" — hàm return **trước** dòng cập nhật
  `last_commanded_mode` (xem thứ tự code trong `handle_set_mode()`), nên
  không có "Operator: SET_MODE..." nào được log và L2 không nhận gì.

### TC-UC07-4: Edge — SET_MODE với mode trùng mode hiện tại được ACK ngay lập tức (không PENDING)
- **Loại**: Edge case
- **Liên quan**: UC-07 — nhánh "genuinely idle" (SC-01A) của `lx_fsm_on_set_mode()`
- **Môi trường**: (B) `c_main` + `lx_main 3`
- **Chuẩn bị**: L3 đang `MODE_PEAK_FIXED` (mặc định, chưa từng đổi mode).
- **Các bước**:
  1. Bấm `m` → Lx number `3` → mode `0` (PEAK_FIXED — trùng mode hiện tại).
- **Kết quả mong đợi**: C1 log "C1: SET_MODE to 3 -> ACK" (không phải
  `ACK_PENDING`) — vì `(operating_mode_t)payload->mode == fsm->mode` đúng,
  rơi vào nhánh áp dụng ngay của `lx_fsm_on_set_mode()` (`lx_fsm.c` dòng
  660-669), không set `mode_change_pending`.

---

## UC-08 — Apply a Bounded Clear-Route Override

### TC-UC08-1: Positive — override arterial thật sự giữ đèn xanh vượt quá thời lượng PEAK_FIXED bình thường
- **Loại**: Positive
- **Liên quan**: UC-08 main flow bước 1-8, BR-1, BR-5
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 vừa khởi động, đang ở `PHASE_ARTERIAL_GREEN` với
  `green_elapsed_ms` gần 0 (bấm lệnh càng sớm sau dòng "SIGNAL -> ARTERIAL
  GREEN" càng tốt).
- **Các bước**:
  1. Bấm `o` → prompt "  Lx number (1-6): " nhập `1` → prompt "  target
     movement (0=arterial, 1=connector): " nhập `0` → prompt "  duration_ms
     (1-300000): " nhập `60000`.
  2. Quan sát log L1 liên tục đến giây thứ 62.
- **Kết quả mong đợi**: C1 log "Operator: REQUEST_OVERRIDE(target=1,
  movement=0, duration_ms=60000) submitted" rồi "C1: REQUEST_OVERRIDE to 1
  -> ACK" (không có ped clearance nào đang chạy, không railway, không
  fault → nhánh ACK trực tiếp, `lx_fsm.c` dòng 729-736). **Không** có dòng
  "SIGNAL -> ARTERIAL YELLOW" nào xuất hiện tại mốc 48 giây bình thường —
  vì guard tại `lx_fsm.c` dòng 956-965 giữ nguyên `ARTERIAL_GREEN` suốt
  khi `override_substate==OVR_ACTIVE` và `target_movement==ARTERIAL`.
  Đúng tại giây thứ 60 (hết `override_remaining_ms`), L1 in "Lx 1:
  override cleared/expired - running safe clearance sequence" rồi ngay
  sau đó mới "SIGNAL -> ARTERIAL YELLOW" như bình thường.

### TC-UC08-2: Positive — override connector thật sự ép đổi hướng đèn xanh (đúng yêu cầu review gần nhất)
- **Loại**: Positive
- **Liên quan**: UC-08 main flow bước 5-6 (đèn thật sự đổi hướng, không chỉ bookkeeping)
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 vừa khởi động, đang ở `PHASE_ARTERIAL_GREEN`,
  `green_elapsed_ms≈0`.
- **Các bước**:
  1. Bấm `o` → Lx `1` → target movement `1` (connector) → duration_ms
     `300000` (dùng trần tối đa để chắc chắn còn hiệu lực đến khi tới được
     pha connector).
  2. Quan sát log L1 đến khi thấy "SIGNAL -> CONNECTOR GREEN" (dự kiến tại
     `t≈54s`: 48s arterial green bình thường + 4s yellow + 2s all-red,
     override không rút ngắn các bước này).
  3. Sau khi thấy "CONNECTOR GREEN" được giữ quá mốc 30 giây bình thường
     (đợi thêm ít nhất 10 giây nữa để chắc chắn nó không tự chuyển yellow),
     kết thúc sớm bằng: bấm `c` → prompt "  Lx number (1-6): " nhập `1`.
- **Kết quả mong đợi**: tại `t≈54s`, ranh giới `PHASE_ALL_RED_A_TO_B` áp
  dụng nhánh override (`lx_fsm.c` dòng 243-260): vì
  `override_target_movement==OVERRIDE_MOVEMENT_CONNECTOR`, pha kế tiếp bị
  ép thành `PHASE_CONNECTOR_GREEN` — log in "Lx 1: SIGNAL -> CONNECTOR
  GREEN" đúng lúc này (không phải theo lịch PEAK_FIXED thông thường, vì
  PEAK_FIXED bình thường **cũng** đi tới CONNECTOR_GREEN tại đây — điểm
  khác biệt thật sự quan sát được là bước tiếp theo: pha này **không**
  chuyển "CONNECTOR YELLOW" tại mốc 30s như bình thường, do guard dòng
  997-1006 giữ nó lại). Tại bước 3, C1 log "Operator: CANCEL_OVERRIDE(target=1)
  submitted" rồi "C1: CANCEL_OVERRIDE to 1 -> ACK"; L1 ngay lập tức in "Lx
  1: override cleared/expired - running safe clearance sequence" rồi mới
  "SIGNAL -> CONNECTOR YELLOW" — chứng minh đèn xanh connector thật sự chỉ
  do override giữ, không phải trùng hợp với lịch PEAK_FIXED.

### TC-UC08-3: Negative/alt 3.3 — duration bằng 0 bị Central từ chối trước khi tới controller
- **Loại**: Negative
- **Liên quan**: UC-08 alt 3.3 ("Requested duration is invalid or unbounded"), BR-3
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 đang chạy bình thường, không có override nào.
- **Các bước**:
  1. Bấm `o` → Lx `1` → target movement `0` → duration_ms `0`.
- **Kết quả mong đợi**: C1 log "Operator: REQUEST_OVERRIDE(target=1,
  movement=0, duration_ms=0) rejected by Central pre-check,
  reason=INVALID_DURATION - not forwarded to the controller" — request
  **không bao giờ** được gửi tới L1 (`c_mode_eng_validate_override_request()`
  chặn trước, `c_operator.c` dòng 248-254). L1 không có bất kỳ log override
  nào.

### TC-UC08-4: Negative/alt 3.2 — railway pre-emption đang hoạt động khiến override bị NACK
- **Loại**: Negative
- **Liên quan**: UC-08 alt 3.2 ("Railway pre-emption conflicts with the requested movement"), BR-2
- **Môi trường**: (B) `c_main` + `lx_main 1` + `rlx_main 1`
- **Chuẩn bị**: kích hoạt pre-emption trên L1 trước (bấm `0` trên RL1, đợi
  ít nhất 2 giây để L1 nhận được `CROSSING_STATUS(WARNING)`).
- **Các bước**:
  1. Sau khi chắc chắn L1 đang `RAILWAY_PREEMPTION`, bấm `o` → Lx `1` →
     target movement `0` → duration_ms `10000`.
- **Kết quả mong đợi**: C1 log "C1: REQUEST_OVERRIDE to 1 -> NACK
  reason=RAILWAY_CONFLICT" (`lx_fsm.c` dòng 705-709, kiểm tra
  `SUPERVISORY_RAILWAY_PREEMPTION` trước cả kiểm tra ped clearance). L1
  vẫn giữ nguyên `RAILWAY_PREEMPTION`, không có override nào được kích
  hoạt.

### TC-UC08-5: Negative/alt 3.1 — override bị hoãn (ACK_PENDING) khi đang phục vụ người đi bộ, rồi tự kích hoạt sau khi xong
- **Loại**: Negative
- **Liên quan**: UC-08 alt 3.1 ("Pedestrian clearance is in progress"), BR-6
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 ở `PHASE_ARTERIAL_GREEN`.
- **Các bước**:
  1. Trên `lx_sensor` của L1, bấm `1` (ped side 0) để bắt đầu WALK/FDW
     (kéo dài tổng 10 giây).
  2. Ngay trong lúc WALK đang chạy (ví dụ 1 giây sau khi bấm `1`), trên C1
     bấm `o` → Lx `1` → target movement `0` → duration_ms `15000`.
  3. Quan sát log C1 và L1 đến giây thứ 12.
- **Kết quả mong đợi**: C1 log "C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING"
  (vì `fsm->ped_clearance_active==1` → nhánh `OVR_PENDING_CLEARANCE`,
  `lx_fsm.c` dòng 713-728). `override_remaining_ms` bắt đầu đếm lùi từ
  15000ms **ngay cả khi đang pending** (dòng 895-902). Khi WALK/FDW hoàn
  tất tại giây thứ 10 (tính từ lúc bấm `1`), `ped_clearance_active` về 0
  và ngay tick kế tiếp `override_substate` tự chuyển `OVR_ACTIVE` (dòng
  935-938) **mà không có reply thứ hai nào được gửi** — chỉ có thể quan
  sát gián tiếp qua việc `ARTERIAL_GREEN` không thoát tại mốc 48s bình
  thường (nếu chờ đến đó) hoặc qua bảng HMI của C1 (cột SUPERVISORY chuyển
  từ 3 sang 2 đúng vào thời điểm này nếu STATUS/HEARTBEAT tiếp theo của L1
  được ghi nhận).

### TC-UC08-6: Edge — biên 300000/300001 ms và việc gửi REQUEST_OVERRIDE thứ hai khi đã có override đang ACTIVE
- **Loại**: Edge case
- **Liên quan**: UC-08 BR-5 ("bounded and auto-expiring"), BR-7, compliance-audit fix chống ghi đè override đang chạy
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 đang chạy bình thường, không có override nào.
- **Các bước**:
  1. Bấm `o` → Lx `1` → target movement `0` → duration_ms `300001`.
  2. Bấm `o` → Lx `1` → target movement `0` → duration_ms `300000`.
  3. Ngay sau khi bước 2 vừa được ACK (còn đang `OVR_ACTIVE`), bấm `o` →
     Lx `1` → target movement `1` → duration_ms `5000`.
- **Kết quả mong đợi**: bước 1 bị Central pre-check chặn ("... rejected by
  Central pre-check, reason=INVALID_DURATION - not forwarded ...", vì
  300001 > 300000 = `LX_OVERRIDE_DURATION_CAP_MS`). Bước 2 được Central
  forward và L1 trả `ACK` (300000 đúng bằng cap, hợp lệ — biên đóng, không
  bị từ chối). Bước 3 vượt qua được Central pre-check (target/duration đều
  hợp lệ) nhưng bị **L1** từ chối: "C1: REQUEST_OVERRIDE to 1 -> NACK
  reason=OUT_OF_RANGE" — vì `fsm->supervisory==SUPERVISORY_CENTRAL_OVERRIDE`
  đã đúng (từ bước 2), rơi vào guard "không cho REQUEST_OVERRIDE thứ hai
  đè lên override đang có, phải dùng RENEW_OVERRIDE" (`lx_fsm.c` dòng
  686-700). Override từ bước 2 (target=arterial, 300000ms) vẫn tiếp tục
  chạy, không bị ảnh hưởng bởi yêu cầu bị NACK ở bước 3.

---

## UC-09 — Monitor Network Status and Faults

### TC-UC09-1: Positive — bảng trạng thái hiển thị đầy đủ 9 controller sau khi mọi node đã báo cáo
- **Loại**: Positive
- **Liên quan**: UC-09 main flow bước 1-8, BR-1
- **Môi trường**: (B) `c_main` + `lx_main 1`..`lx_main 6` + `rlx_main 1`..`rlx_main 3` (tổng 10 tiến trình trên cùng máy)
- **Chuẩn bị**: khởi động theo đúng thứ tự khuyến nghị: `c_main` trước, rồi
  3 `rlx_main`, rồi 6 `lx_main` (mục 2.3 `QNX_DEPLOYMENT_RUN_GUIDE.md`).
- **Các bước**:
  1. Khởi động đủ 10 tiến trình như trên.
  2. Chờ 3 giây (đủ ít nhất 2-3 heartbeat tick).
  3. Đọc bảng "---- C1 network status ----" in ra mỗi giây trên console
     của `c_main`.
- **Kết quả mong đợi**: bảng có đúng 9 dòng (L1..L6, RL1..RL3), cột
  `AVAILABILITY` = `AVAILABLE` cho cả 9 dòng, cột `ROLE` đúng
  `INTERSECTION`/`RAILWAY`, cột `MODE`/`PHASE` của các Lx = `0` (`MODE_PEAK_FIXED`)
  và `0` (`PHASE_ARTERIAL_GREEN`, vừa khởi động), cột `CROSSING_STATE` của
  các RLx = `0` (`CROSSING_OPEN`), cột `SUPERVISORY` = `3`
  (`NORMAL_OPERATION`) cho các Lx.

### TC-UC09-2: Negative/alt 5.1 — controller chưa từng khởi động bị đánh dấu UNAVAILABLE sau đúng 3 giây
- **Loại**: Negative
- **Liên quan**: UC-09 alt 5.1 ("A controller becomes unreachable"), BR-2 (PA-07)
- **Môi trường**: (B) `c_main` + `lx_main 1` (các controller còn lại
  **không** chạy)
- **Chuẩn bị**: chỉ khởi động `c_main` rồi `lx_main 1`.
- **Các bước**:
  1. Khởi động `c_main`.
  2. Ngay sau đó khởi động `lx_main 1`.
  3. Quan sát log C1 trong 5 giây.
- **Kết quả mong đợi**: đúng tại tick thứ 3 (khoảng `t=3s` kể từ lúc
  `c_main` start, vì `missed_heartbeat_ticks` tăng mỗi giây và không
  controller nào khác từng gửi báo cáo để reset nó về 0), C1 in đúng 8
  dòng liên tiếp dạng "Controller N marked UNAVAILABLE - missed 3
  consecutive heartbeats (PA-07)" cho N = 2,3,4,5,6,7,8,9 (L2..L6,
  RL1..RL3) — **không** có dòng này cho controller 1 (L1), vì L1 đã gửi
  HEARTBEAT đầu tiên trong vòng 1 giây sau khi khởi động, reset
  `missed_heartbeat_ticks` về 0 liên tục. Bảng HMI xác nhận L1 = AVAILABLE,
  8 dòng còn lại = UNAVAILABLE.

### TC-UC09-3: Positive/alt 2.1 — controller reconnect thay thế view cũ chỉ sau khi có báo cáo đầy đủ
- **Loại**: Positive
- **Liên quan**: UC-09 alt 2.1 ("A previously unavailable controller reconnects"), BR-3 (PA-08)
- **Môi trường**: (B) tiếp tục từ TC-UC09-2
- **Chuẩn bị**: trạng thái cuối TC-UC09-2 (L2 đang UNAVAILABLE, chưa từng
  chạy).
- **Các bước**:
  1. Khởi động `lx_main 2` (muộn).
  2. Quan sát log/bảng HMI trong 2 giây tiếp theo.
- **Kết quả mong đợi**: trong vòng 1 giây (heartbeat đầu tiên của L2), dòng
  L2 trong bảng HMI chuyển ngay từ `UNAVAILABLE` sang `AVAILABLE` với dữ
  liệu tươi (`MODE`/`PHASE` đúng trạng thái khởi động thật của L2, không
  phải dữ liệu rác) — vì `c_server_record_status()` set
  `missed_heartbeat_ticks=0` và `marked_unavailable=0` ngay khi nhận được
  `MSG_HEARTBEAT` đầu tiên (`c_server.c` dòng 10-11), không cần đợi thêm
  chu kỳ nào.

### TC-UC09-4: Negative/alt 6.1 — fault đường sắt hiển thị trên Central độc lập với an toàn cục bộ đã có từ trước
- **Loại**: Negative
- **Liên quan**: UC-09 alt 6.1 ("A railway fault report arrives")
- **Môi trường**: (B) `c_main` + `rlx_main 1`
- **Chuẩn bị**: `c_main` và `rlx_main 1` đang chạy bình thường.
- **Các bước**:
  1. Trên RL1, bấm `x` rồi `0` (như TC-UC06-1), đợi đến `t=20s` để vào
     `FAULT`.
  2. Quan sát bảng HMI của C1 trong 3 giây sau đó.
- **Kết quả mong đợi**: RL1 đã tự đóng chắn và giữ STOP ngay tại `t=20s`
  **trước khi** `FAULT_REPORT` kịp tới C1 (an toàn cục bộ độc lập, RC-10).
  Trong vòng 1 giây sau đó, dòng RL1 trên bảng HMI của C1 cập nhật
  `CROSSING_STATE=3` (`CROSSING_FAULT`) và `FAULTS=0x1`; C1 chỉ đóng vai
  trò hiển thị, không có bất kỳ hành động actuate nào được gửi ngược lại
  RL1 (đúng BR-1 "Central chỉ giám sát 9 controller, không điều khiển trực
  tiếp thiết bị").

---

## UC-10 — Continue Local Operation During Central Link Loss

### TC-UC10-1: Positive — Lx tiếp tục vận hành đúng lịch dù không có Central
- **Loại**: Positive
- **Liên quan**: UC-10 main flow bước 1-6, alt 7.1
- **Môi trường**: (A) chỉ `lx_main 1` (không chạy `c_main`)
- **Chuẩn bị**: không khởi động `c_main`.
- **Các bước**:
  1. Chạy `/tmp/lx_main 1`.
  2. Quan sát log liên tục trong 90 giây (đúng 1 `LX_CYCLE_LENGTH_MS`).
- **Kết quả mong đợi**: chuỗi "SIGNAL -> ..." xuất hiện đúng y hệt
  TC-UC01-1 (48s/4s/2s/30s/4s/2s), không có bất kỳ khoảng dừng/treo nào dù
  mỗi giây `lx_comm_send_heartbeat()` gọi `ipc_client_post()` gửi tới C1
  và thất bại (`name_open("traffic/c1")` không tìm thấy tiến trình nào) —
  log phụ "Lx: HEARTBEAT to 0 failed to send" (từ `on_heartbeat_reply()`
  trong `lx_comm.c`) có thể xuất hiện mỗi giây nhưng **không** làm chậm
  hay chặn vòng lặp pha chính, vì gửi heartbeat luôn qua hàng đợi
  non-blocking (`ipc_client_post()`), tách biệt hoàn toàn khỏi luồng server
  chạy `lx_fsm_on_phase_timer()`.

### TC-UC10-2: Positive/alt 6.1 — bảo vệ đường sắt vẫn hoạt động đầy đủ dù Central không tồn tại
- **Loại**: Positive
- **Liên quan**: UC-10 alt 6.1 ("Railway event occurs while disconnected")
- **Môi trường**: (B) `lx_main 1` + `rlx_main 1` (RL1 kề L1), **không**
  chạy `c_main`
- **Chuẩn bị**: chỉ 2 tiến trình trên, không có C1.
- **Các bước**:
  1. Trên RL1, bấm `0` tại `t=0`.
  2. Quan sát log L1 và RL1 liên tục đến `t=55s`.
- **Kết quả mong đợi**: toàn bộ trình tự WARNING→CLOSING→CLOSED→
  TRAIN_PRESENT→OPENING→OPEN của RL1 diễn ra đúng y hệt TC-UC04-1 (các mốc
  5s/8s/28s/48s/51s), và L1 vẫn nhận được `MSG_CROSSING_STATUS` trực tiếp
  từ RL1 (gửi thẳng qua Qnet, không đi qua C1 — `rlx_comm.c`'s
  `send_crossing_status()` gọi trực tiếp tới `adjacent_lx[]`), thể hiện
  qua việc log L1 **không** có "SIGNAL -> CONNECTOR GREEN" nào trong suốt
  cửa sổ pre-emption, giống hệt TC-UC01-5 — chứng minh bảo vệ đường sắt
  hoàn toàn không phụ thuộc Central (RC-05/RC-10 áp dụng xuyên UC-04 lẫn
  UC-10).

### TC-UC10-3: Positive — trạng thái đầy đủ được gửi lại ngay khi Central khởi động lại (không cần L1 restart)
- **Loại**: Positive
- **Liên quan**: UC-10 main flow bước 7-9, BR-3 (PA-08)
- **Môi trường**: (B) `lx_main 1` khởi động trước, `c_main` khởi động sau
- **Chuẩn bị**: chạy `lx_main 1` một mình trước (như TC-UC10-1), để nó
  chạy độc lập ít nhất 10 giây.
- **Các bước**:
  1. Sau khi L1 đã chạy độc lập ≥10 giây, khởi động `/tmp/c_main`.
  2. Quan sát bảng HMI của C1 trong 2 giây sau khi nó khởi động xong.
- **Kết quả mong đợi**: vì `ipc_attach()` của C1 dùng
  `NAME_FLAG_ATTACH_GLOBAL` và L1 chỉ mới bắt đầu `name_open("traffic/c1")`
  thành công **từ heartbeat tick kế tiếp sau khi C1 đã attach xong**, dòng
  L1 xuất hiện trên bảng HMI của C1 trong vòng 1 giây kể từ khi C1 sẵn
  sàng, với `AVAILABILITY=AVAILABLE` và dữ liệu phản ánh đúng trạng thái
  **hiện tại** của L1 (ví dụ nếu L1 đang ở `CONNECTOR_GREEN` sau 10 giây
  chạy độc lập thì HMI phải hiện đúng `PHASE` tương ứng, không phải giá
  trị mặc định lúc khởi động) — đúng yêu cầu "gửi state hiện tại trước khi
  nhận lệnh mới" của BR-3 (lưu ý: cài đặt hiện tại gửi state đầy đủ ngay
  trong mỗi `STATUS`/`HEARTBEAT` bình thường chứ không có bước "đồng bộ"
  tách riêng, nên không có cơ chế **chặn** lệnh mới trong lúc chờ đồng bộ
  — đây là điểm đơn giản hoá đã biết so với văn bản đặc tả, không phải
  lỗi).

### TC-UC10-4: Edge — REQUEST_OVERRIDE gửi tới controller chưa khởi động vẫn được Central ghi nhận "in-flight" dù gửi thất bại
- **Loại**: Edge case
- **Liên quan**: UC-10 tương phản với UC-08 khi peer không khả dụng — phát hiện một điểm bookkeeping lạc quan đáng chú ý
- **Môi trường**: (B) `c_main` chạy, `lx_main 2` **không** chạy
- **Chuẩn bị**: chỉ `c_main` đang chạy, L2 chưa từng khởi động.
- **Các bước**:
  1. Bấm `o` → Lx number `2` → target movement `0` → duration_ms `10000`.
- **Kết quả mong đợi**: Central pre-check chấp nhận (target=2 hợp lệ,
  duration hợp lệ) → log "Operator: REQUEST_OVERRIDE(target=2, movement=0,
  duration_ms=10000) submitted", và **ngay lập tức** (trước khi biết kết
  quả gửi) `mode_eng->controllers[1].override_in_flight` được set = 1
  (bookkeeping lạc quan, `c_operator.c` dòng 227-246, comment "Not
  synchronised with the Lx's own eventual ACK/NACK"). Vì L2 không chạy,
  `ipc_client_post()` gửi thất bại → log "C1: REQUEST_OVERRIDE to 2 send
  failed (peer unreachable or send error)". Bảng HMI của C1 vẫn hiển thị
  L2 = `UNAVAILABLE`, nhưng nội bộ Central vẫn coi override là "in flight"
  cho đến khi operator tự gọi `c` (CANCEL_OVERRIDE) để xoá cờ này theo
  cách thủ công — không có cơ chế tự động phát hiện gửi thất bại để rollback
  `override_in_flight`. Đây là hành vi đúng như code hiện tại, được ghi
  nhận để lưu ý khi vận hành thật (operator không nên tin tưởng tuyệt đối
  cờ này khi controller đích đang mất kết nối).
