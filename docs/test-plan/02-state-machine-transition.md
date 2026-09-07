# Test Plan 02 — Chuyển tiếp trạng thái (State Machine Transitions)

Tài liệu này liệt kê test case cho từng transition trong `STATE_CHARTS.md`
(SC-01A/B/C, SC-02, SC-03A/B, SC-04A/B, SC-05), đối chiếu trực tiếp với mã
nguồn thật tại thời điểm viết tài liệu này:

- `app/intersection/src/lx_fsm.c`, `app/intersection/includes/lx_timer.h`
- `app/intersection/src/lx_sensor.c`, `app/intersection/src/lx_signal.c`
- `app/railway/src/rlx_fsm.c`, `app/railway/includes/rlx_timer.h`
- `app/railway/src/rlx_sensor.c`, `app/railway/src/rlx_gate.c`, `app/railway/src/rlx_signal.c`
- `app/central/src/c_watchdog_mon.c`, `app/central/src/c_operator.c`,
  `app/central/src/c_hmi.c`, `app/central/src/c_server.c`, `app/central/src/c_logger.c`
- `app/shared/includes/sys_types.h`

Mọi hằng số thời gian, tên phím, tên trường log trong tài liệu này được lấy
trực tiếp từ các file trên — không suy đoán. Ở những chỗ code có giới hạn/lỗ
hổng đã biết (không có cách kích hoạt transition qua giao diện hiện có), test
case được ghi rõ là "Known gap" thay vì bịa ra một cách kích hoạt không tồn tại.

## 0. Quy ước chung

### 0.1 Ba môi trường test A/B/C

| Ký hiệu | Mô tả | Khi nào dùng |
|---|---|---|
| **(A) 1 node đơn** | Chỉ chạy **một** tiến trình (`lx_main N` hoặc `rlx_main N`) trên một máy QNX, không có `c_main`. Điều khiển hoàn toàn qua bàn phím sensor của chính tiến trình đó (`lx_sensor.c` / `rlx_sensor.c`). | Test các transition nội bộ của SC-01, SC-02, SC-04 không cần Central và không cần giao tiếp giữa Lx/RLx. |
| **(B) Nhiều node cùng máy QNX** | Nhiều tiến trình (`c_main`, `lx_main 1`, `rlx_main 1`, …) chạy trên **cùng một** target QNX (nhiều cửa sổ/console trên cùng máy). `TRAFFIC_NODE_MAP` không cần export (mặc định "same node"). | Test override (SC-03B) — bắt buộc phải có `c_main` vì chỉ `c_operator.c` mới gửi được `REQUEST_OVERRIDE`/`RENEW_OVERRIDE`/`CANCEL_OVERRIDE`. Test SC-03A phần tương tác Lx–RLx (RAILWAY_PREEMPTION), SC-05. |
| **(C) Nhiều máy/VM QNX qua mạng thật** | Từng vai trò chạy trên VM/PC vật lý khác nhau, kết nối qua Qnet, có export `TRAFFIC_NODE_MAP` theo đúng `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` (Case 1/2/3). | Lặp lại các test case (B) quan trọng nhất (đặc biệt SC-03A test regression #1, SC-05) trên topology mạng thật trước khi nghiệm thu cuối kỳ, vì độ trễ Qnet thật có thể bộc lộ race-condition không thấy được khi chạy same-node. |

Mọi test case bên dưới ghi rõ môi trường tối thiểu cần dùng ở dòng
**Môi trường**. Một test được đánh dấu (B) có thể luôn được lặp lại ở (C)
nếu nhóm có đủ máy — khuyến nghị làm vậy cho các test có nhãn "Critical
regression".

### 0.2 Build & khởi động tiến trình

```
make        # build/bin/c_main, build/bin/lx_main, build/bin/rlx_main
/tmp/c_main            # Central, không có tham số
/tmp/lx_main <1-6>     # chọn L1..L6
/tmp/rlx_main <1-3>    # chọn RL1..RL3
```

Thứ tự khởi động khuyến nghị (giống `QNX_DEPLOYMENT_RUN_GUIDE.md`): `c_main`
trước, rồi `rlx_main`, rồi `lx_main`. Với môi trường (C), export
`TRAFFIC_NODE_MAP` trong **mọi** shell trước khi chạy binary theo đúng mục
2.2 của guide đó. Sơ đồ kề RLx–Lx cố định trong code
(`app/railway/src/rlx_comm.c`, mảng `ADJACENCY`):

| RLx | Kề với |
|---|---|
| RL1 | L1, L2 |
| RL2 | L3, L4 |
| RL3 | L5, L6 |

### 0.3 Bảng phím tắt

**`lx_sensor.c` (bàn phím của mỗi tiến trình `lx_main`)**

| Phím | Tác dụng |
|---|---|
| `a` / `A` | Xe có mặt / rời khỏi approach arterial |
| `c` / `C` | Xe có mặt / rời khỏi approach connector |
| `1`/`2`/`3`/`4` | Bấm nút người đi bộ side 0/1/2/3 |
| `w` / `W` | Bật / tắt queue-warning (CC-01) |

**`rlx_sensor.c` (bàn phím của mỗi tiến trình `rlx_main`)**

| Phím | Tác dụng |
|---|---|
| `0` / `1` | TRAIN_APPROACHING hướng 0 / hướng 1 |
| `x` | Ép lần đóng/mở cổng chắn **kế tiếp** không bao giờ xác nhận (demo lỗi RC-06) |
| `f` | Demo-only: gọi thẳng `rlx_fsm_on_fault_clear()` tại chỗ (bỏ qua đường IPC `MSG_REQUEST_FAULT_CLEAR` thật từ Central) |

**`c_operator.c` (console của `c_main`, chỉ tồn tại khi Central chạy)**

| Phím | Tác dụng |
|---|---|
| `m` | `SET_MODE` cho một Lx (nhập số Lx 1-6, mode 0=PEAK_FIXED/1=OFF_PEAK_SENSOR) |
| `t` | Broadcast `SET_TIMING_PROFILE` cho chain R1 hoặc R2 |
| `o` | `REQUEST_OVERRIDE` (nhập Lx, `target_movement` 0=arterial/1=connector, `duration_ms`) |
| `r` | `RENEW_OVERRIDE` (nhập Lx, `extend_duration_ms`, 0 = giữ nguyên thời lượng cũ) |
| `c` | `CANCEL_OVERRIDE` (nhập Lx) |
| `f` | `REQUEST_FAULT_CLEAR` cho một RLx (nhập RLx 1-3) |

### 0.4 Cách đọc log

- Mỗi `lx_main`/`rlx_main` in trực tiếp ra stdout của chính nó (không có
  timestamp) — ví dụ `Lx 1: SIGNAL -> ARTERIAL GREEN`,
  `RLx: commanding gates DOWN (simulated motion, 3000 ms)`.
- `c_main` in ra stdout **và** ghi vào `central_log.txt` (cùng thư mục chạy
  `c_main`) với định dạng `[YYYY-MM-DD HH:MM:SS] <nội dung>`
  (`c_logger_log()`).
- Bảng trạng thái mạng của `c_main` (`c_hmi_render()`, cột
  `ID ROLE MODE PHASE CROSSING_STATE SUPERVISORY FAULTS SENSOR OVERRIDE
  AVAILABILITY`) tự động in lại **mỗi 1 giây** (do `IPC_PULSE_HEARTBEAT_TICK`
  1 Hz) — không cần bấm phím gì để refresh. Các cột đó là số nguyên thô,
  tra theo bảng mã dưới đây.

**Bảng mã hiển thị trên C1**

| Trường | Giá trị / ý nghĩa |
|---|---|
| MODE | `0`=PEAK_FIXED, `1`=OFF_PEAK_SENSOR |
| PHASE | `0`=ARTERIAL_GREEN, `1`=ARTERIAL_YELLOW, `2`=ALL_RED_A_TO_B, `3`=CONNECTOR_GREEN, `4`=CONNECTOR_YELLOW, `5`=ALL_RED_B_TO_A |
| CROSSING_STATE | `0`=OPEN, `1`=WARNING, `2`=CLOSED, `3`=FAULT |
| SUPERVISORY | `0`=FAULT_SAFE, `1`=RAILWAY_PREEMPTION, `2`=CENTRAL_OVERRIDE, `3`=NORMAL_OPERATION |
| OVERRIDE | `1` nếu `override_substate==OVR_ACTIVE`, ngược lại `0` (không phân biệt được `OVR_PENDING_CLEARANCE` qua cột này — phải xem log `c_operator`/console Lx) |
| AVAILABILITY | `AVAILABLE` / `UNAVAILABLE` (`marked_unavailable`, PA-07) |

### 0.5 Hằng số thời gian dùng xuyên suốt tài liệu

Từ `app/intersection/includes/lx_timer.h`:

| Hằng số | Giá trị |
|---|---|
| `LX_PEAK_ARTERIAL_GREEN_MS` | 48000 ms |
| `LX_PEAK_CONNECTOR_GREEN_MS` | 30000 ms |
| `LX_YELLOW_MS` | 4000 ms |
| `LX_ALL_RED_MS` | 2000 ms |
| `LX_MIN_GREEN_MS` | 8000 ms |
| `LX_MAX_GREEN_MS` | 40000 ms |
| `LX_EXTENSION_MS` | 4000 ms |
| `LX_OVERRIDE_DURATION_CAP_MS` | 300000 ms (5 phút, PA-11) |
| `LX_PHASE_TICK_MS` | 100 ms |
| `LX_WALK_MS` | 6000 ms |
| `LX_FLASHING_DONT_WALK_MS` | 4000 ms |
| `LX_DRAIN_MAX_EXTENSION_MS` | 60000 ms |
| `LX_CYCLE_LENGTH_MS` | 90000 ms (48+4+2+30+4+2) |

Từ `app/railway/includes/rlx_timer.h`:

| Hằng số | Giá trị |
|---|---|
| `RLX_WARNING_TO_CLOSING_MS` | 5000 ms |
| `RLX_CLOSING_DEADLINE_MS` | 15000 ms |
| `RLX_EXPECTED_ARRIVAL_MS` | 20000 ms |
| `RLX_OCCUPANCY_WINDOW_MS` | 20000 ms |
| `RLX_OPENING_DEADLINE_MS` | 15000 ms |

Từ `app/railway/includes/rlx_gate.h`: `RLX_GATE_MOTION_MS` = 3000 ms (thời
gian mô phỏng cổng di chuyển) và tick railway = 1000 ms/lần
(`rlx_fsm_on_tick()` được gọi từ `IPC_PULSE_RAILWAY_WARNING` mỗi 1 s).

---

## 1. SC-01A — Vehicle-Signal Mode Selection Overview

### TC-SC01A-1: SET_MODE bị hoãn đến đúng ranh giới an toàn (safe phase boundary)
- **Loại**: Positive
- **Liên quan**: SC-01A, `PEAK_FIXED -> mode_selection : pending mode change [safe phase boundary]`
- **Môi trường**: (B) `c_main` + `lx_main 1`
- **Chuẩn bị**: L1 ở `MODE_PEAK_FIXED` (mặc định lúc khởi động — `lx_fsm_init()`), đang ở `PHASE_ARTERIAL_GREEN`.
- **Các bước**:
  1. Trên console C1, bấm `m`, nhập Lx = `1`, mode = `1` (OFF_PEAK_SENSOR) ngay khi L1 vừa mới vào ARTERIAL_GREEN (quan sát log `Lx 1: SIGNAL -> ARTERIAL GREEN` trên terminal L1).
  2. Quan sát reply trả về cho C1 (`c_comm.c` log `RESULT_ACK_PENDING`, vì mode mới khác mode hiện tại — `lx_fsm_on_set_mode()`).
  3. Chờ đúng đến khi L1 in `ARTERIAL YELLOW` (48s sau bước 1) rồi `ALL RED (A to B)` (thêm 4s), không được đổi mode ở hai bước này.
  4. Chờ thêm 2s (tổng ~54s từ bước 1) đến khi L1 chuyển tiếp ranh giới `ALL_RED_A_TO_B -> boundary_after_arterial`.
- **Kết quả mong đợi**: Mode chỉ đổi thực sự tại đúng thời điểm `ALL_RED_A_TO_B` xử lý xong (trong `lx_fsm_advance_phase_locked()`, nhánh `case PHASE_ALL_RED_A_TO_B`) — L1 vào `CONNECTOR_GREEN` như bình thường (vì `boundary_after_arterial --> CONNECTOR_GREEN` không phụ thuộc mode mới), nhưng phase **tiếp theo sau đó** (khi quay lại ARTERIAL_GREEN) đã chạy theo timing OFF_PEAK_SENSOR (không còn cố định 48s/30s nữa). Trạng thái C1 (`c_hmi_render`) cho L1 đổi MODE từ `0` sang `1` đúng ngay sau mốc 54s, không sớm hơn.

### TC-SC01A-2: Yêu cầu đổi sang mode đang chạy sẵn → ACK ngay, không hoãn
- **Loại**: Edge case
- **Liên quan**: SC-01A, nhánh no-op trong `lx_fsm_on_set_mode()` (yêu cầu trùng `fsm->mode`)
- **Môi trường**: (B)
- **Chuẩn bị**: L1 đang `MODE_PEAK_FIXED`.
- **Các bước**: Bấm `m`, Lx=`1`, mode=`0` (PEAK_FIXED — trùng mode hiện tại).
- **Kết quả mong đợi**: `c_operator` log `RESULT_ACK` (không phải `ACK_PENDING`). `mode_change_pending` không được set (test gián tiếp: gửi tiếp một `SET_MODE` khác ngay sau đó với mode=1 phải hoạt động bình thường như một yêu cầu hoàn toàn mới, không bị lẫn với yêu cầu cũ). Phase hiện tại của L1 không bị gián đoạn — log signal vẫn tiếp diễn đúng nhịp.

### TC-SC01A-3: SET_MODE bị NACK khi đang FAULT_SAFE
- **Loại**: Negative
- **Liên quan**: SC-01A / SC-03A giao nhau — `lx_fsm_check_fault_locked()` được gọi đầu `lx_fsm_on_set_mode()`
- **Môi trường**: (B)
- **Chuẩn bị**: Đưa L1 vào FAULT_SAFE bằng watchdog thật: tạm dừng toàn bộ tiến trình `lx_main 1` bằng `kill -STOP <pid>` trong hơn 2 giây (ngưỡng `LX_WATCHDOG_CHECK_INTERVAL_S`=2s trong `lx_watchdog.c`) rồi `kill -CONT <pid>` — khi resume, `lx_watchdog_thread` phát hiện `phase_tick_counter` không đổi và gọi `lx_fsm_report_watchdog_trip()`.
- **Các bước**:
  1. `kill -STOP <pid lx_main 1>`, đợi 3s, `kill -CONT <pid>`.
  2. Quan sát log `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)` và `Lx 1: FAULT_SAFE - holding safe outputs (all-red/dark)`.
  3. Trên C1, bấm `m`, Lx=`1`, mode=`1`.
- **Kết quả mong đợi**: `reply->result = RESULT_NACK`, `reply->reason = NACK_REASON_FAULT_ACTIVE`. Cột SUPERVISORY của L1 trên C1 = `0` (FAULT_SAFE) và không đổi.

---

## 2. SC-01B — PEAK_FIXED Phase Detail

### TC-SC01B-1: Chu kỳ cố định đúng 90s (48+4+2+30+4+2)
- **Loại**: Positive
- **Liên quan**: SC-01B, toàn bộ chuỗi `ARTERIAL_GREEN -> ... -> ALL_RED_B_TO_A -> ARTERIAL_GREEN`
- **Môi trường**: (A) `lx_main 1` đơn lẻ
- **Chuẩn bị**: L1 mặc định `MODE_PEAK_FIXED`, không cần bấm phím sensor nào (TL-01/TL-02: sensor không ảnh hưởng PEAK_FIXED).
- **Các bước**: Bấm giờ (stopwatch) từ dòng log `Lx 1: SIGNAL -> ARTERIAL GREEN` đầu tiên, ghi lại timestamp tương đối của từng dòng log tiếp theo cho đến dòng `ARTERIAL GREEN` kế tiếp.
- **Kết quả mong đợi**: Thứ tự và độ trễ giữa các dòng log đúng:
  `ARTERIAL GREEN` (t=0) → `ARTERIAL YELLOW` (t≈48.0s) → `ALL RED (A to B)` (t≈52.0s) → `CONNECTOR GREEN` (t≈54.0s) → `CONNECTOR YELLOW` (t≈84.0s) → `ALL RED (B to A)` (t≈88.0s) → `ARTERIAL GREEN` (t≈90.0s). Sai số cho phép ±1 tick (100ms) do granularity của `LX_PHASE_TICK_MS`.

### TC-SC01B-2: Sensor không rút ngắn/kéo dài PEAK_FIXED
- **Loại**: Negative (control test)
- **Liên quan**: SC-01B, ghi chú "Ordinary approach-presence sensors ... never extend or shorten this fixed cycle (TL-01, TL-02)"
- **Môi trường**: (A)
- **Chuẩn bị**: L1 ở `MODE_PEAK_FIXED`, vừa vào `ARTERIAL_GREEN`.
- **Các bước**:
  1. Ngay sau khi vào ARTERIAL_GREEN, bấm liên tục `a A a A c C` (bật/tắt cả hai approach nhiều lần).
  2. Đợi đủ 48s.
- **Kết quả mong đợi**: `ARTERIAL_GREEN -> ARTERIAL_YELLOW` vẫn xảy ra ở đúng t≈48.0s, không sớm hơn dù không có `arterial_vehicle_demand` (đã set về 0 bằng phím `A` cuối), không muộn hơn dù `connector_vehicle_demand`=1 (phím `c` cuối). Điều này xác nhận nhánh `if (fsm->mode == MODE_PEAK_FIXED)` trong `lx_fsm_on_phase_timer()` (case `PHASE_ARTERIAL_GREEN`) chỉ so `green_elapsed_ms` với `lx_timer_peak_green_duration_ms()`, không đọc `arterial_vehicle_demand`/`connector_vehicle_demand`.

### TC-SC01B-3: Biên chính xác 48000ms (không sớm 100ms, không muộn)
- **Loại**: Edge case
- **Liên quan**: SC-01B, `ARTERIAL_GREEN --> ARTERIAL_YELLOW : after 48 s`
- **Môi trường**: (A)
- **Chuẩn bị**: L1 `MODE_PEAK_FIXED`, vừa vào ARTERIAL_GREEN.
- **Các bước**: Do log không có timestamp millisecond, dùng đồng hồ ngoài với độ chính xác ≤50ms; ghi lại thời điểm log `ARTERIAL YELLOW` xuất hiện so với thời điểm log `ARTERIAL GREEN` xuất hiện.
- **Kết quả mong đợi**: Hiệu số nằm trong khoảng [47.9s, 48.1s] — khớp với việc `lx_fsm_on_phase_timer()` cộng dồn đúng 100ms/tick và so sánh `green_elapsed_ms >= 48000` (tick thứ 480 là tick đầu tiên thỏa `>=`, tức đúng 48000ms chứ không phải 47900ms).

---

## 3. SC-01C — OFF_PEAK_SENSOR Phase Detail

### TC-SC01C-1: Minimum green 8s được tôn trọng dù có nhu cầu đối lập ngay lập tức
- **Loại**: Edge case (biên thời gian)
- **Liên quan**: SC-01C, guard `lx_timer_should_exit_green()` — "Never returns 1 (exit) below LX_MIN_GREEN_MS"
- **Môi trường**: (A)
- **Chuẩn bị**: Chuyển L1 sang OFF_PEAK_SENSOR (dùng đường (A) đơn giản nhất: khởi động lại `lx_main 1` rồi chờ nó vào ARTERIAL_GREEN, sau đó — vì (A) không có C1 để gửi SET_MODE — dùng môi trường (B) tối thiểu: `c_main` + `lx_main 1`, bấm `m` Lx=1 mode=1 và đợi áp dụng như TC-SC01A-1). Ngay khi L1 vào `ARTERIAL_GREEN` ở OFF_PEAK_SENSOR (không có `arterial_vehicle_demand`), bấm ngay `c` (connector demand=1) tại t=0.
- **Các bước**:
  1. t=0: bấm `c`.
  2. Theo dõi log mỗi 4s (`LX_EXTENSION_MS`) — đây là chu kỳ tái kiểm tra guard.
- **Kết quả mong đợi**: `ARTERIAL_GREEN` **không** thoát ở t=4000ms dù `own_demand=0` (arterial) và `other_demand=1` (connector) đã đủ điều kiện logic — vì `lx_timer_should_exit_green()` luôn trả `0` khi `elapsed_ms < LX_MIN_GREEN_MS` (8000). Transition `ARTERIAL_GREEN -> ARTERIAL_YELLOW` chỉ xảy ra ở tick t=8000ms (lần kiểm tra `% LX_EXTENSION_MS == 0` đầu tiên mà `elapsed_ms >= 8000`).

### TC-SC01C-2: Không có nhu cầu nào → nghỉ trên arterial green vô thời hạn (DP-04)
- **Loại**: Positive
- **Liên quan**: SC-01C, ghi chú "With no demand anywhere ... rests on arterial green indefinitely (DP-04)"
- **Môi trường**: (B) (dùng đường chuyển mode như TC-SC01C-1)
- **Chuẩn bị**: L1 ở OFF_PEAK_SENSOR, vừa vào ARTERIAL_GREEN, không bấm bất kỳ phím `a/c/1/2/3/4` nào.
- **Các bước**: Chờ quan sát log trong ít nhất 60s (vượt qua cả `LX_MAX_GREEN_MS`=40000ms).
- **Kết quả mong đợi**: Không có dòng log `SIGNAL ->` nào khác xuất hiện — L1 vẫn hiển thị `ARTERIAL GREEN` liên tục quá mốc 40s, vì transition thoát yêu cầu "`connector demand pending`" (`requires_other_demand=1`) không bao giờ đúng khi cả hai approach đều không có nhu cầu — kể cả khi `green >= 40s`, hàm `lx_timer_should_exit_green()` (đọc `own_demand`/`other_demand`, không có nhánh "hết hạn vô điều kiện" cho phía arterial khi `requires_other_demand=1`) vẫn trả `0`.

### TC-SC01C-3: DP-06 anti-starvation — arterial bị ép nhường ở đúng 40s dù vẫn còn nhu cầu riêng
- **Loại**: Edge case (2 điều kiện tranh chấp: nhu cầu arterial liên tục vs. trần 40s)
- **Liên quan**: SC-01C, `ARTERIAL_GREEN --> ARTERIAL_YELLOW : [green >= 8 s and connector demand pending and (no arterial demand or green = 40 s)]`
- **Môi trường**: (B)
- **Chuẩn bị**: L1 OFF_PEAK_SENSOR, vào ARTERIAL_GREEN.
- **Các bước**:
  1. t=0: bấm `a` (arterial demand=1) và `c` (connector demand=1), giữ cả hai bật liên tục suốt bài test (không bấm `A`/`C`).
  2. Theo dõi log mỗi 4s.
- **Kết quả mong đợi**: `ARTERIAL_GREEN` tự gia hạn (self-loop, không log gì mới vì cùng phase) ở các mốc 8s/12s/…/36s (vì `own_demand=1` nên nhánh "no arterial demand" sai, nhưng `green=40s` chưa đúng nên guard vẫn trả 0). Đúng tại t=40000ms, `green_elapsed_ms >= LX_MAX_GREEN_MS` làm nhánh `(no arterial demand or green = 40s)` đúng bất kể `own_demand` — `ARTERIAL_GREEN -> ARTERIAL_YELLOW` xảy ra chính xác ở t≈40.0s, không sớm hơn, không trễ hơn — dù `arterial_vehicle_demand` vẫn đang =1.

### TC-SC01C-4: Connector green cũng bị ép thoát ở 40s dù nhu cầu liên tục (không có anti-starvation ngược)
- **Loại**: Edge case
- **Liên quan**: SC-01C, `CONNECTOR_GREEN --> CONNECTOR_YELLOW : [green >= 8 s and (no connector demand or green = 40 s)]`
- **Môi trường**: (B)
- **Chuẩn bị**: L1 OFF_PEAK_SENSOR, đã tới `CONNECTOR_GREEN` (chờ hết một vòng ARTERIAL trước, hoặc để nhu cầu tự nhiên đưa tới).
- **Các bước**: Giữ `c` bật (connector demand=1) liên tục suốt phase, không tắt.
- **Kết quả mong đợi**: `CONNECTOR_GREEN -> CONNECTOR_YELLOW` vẫn xảy ra đúng ở t≈40.0s kể từ khi vào CONNECTOR_GREEN — khác với arterial, lời gọi `lx_timer_should_exit_green(..., other_demand=0u, requires_other_demand=0u)` trong `lx_fsm_on_phase_timer()` case `PHASE_CONNECTOR_GREEN` không có điều kiện phụ nào khác ngoài "no connector demand or green=40s", nên trần 40s luôn thắng bất kể demand — xác nhận đúng comment "arterial gets service again unconditionally next cycle regardless of its own demand" trong `lx_timer.h`.

---

## 4. SC-02 — Generic Pedestrian-Signal State Chart

### TC-SC02-1: Yêu cầu latch trong pha không tương thích, được phục vụ khi pha tương thích bắt đầu
- **Loại**: Positive
- **Liên quan**: SC-02, `DONT_WALK -> REQUEST_LATCHED -> WALK`
- **Môi trường**: (A) `lx_main 1`, `MODE_PEAK_FIXED`
- **Chuẩn bị**: Đợi L1 vào `CONNECTOR_GREEN` (side 0/1 tương thích với ARTERIAL, không tương thích CONNECTOR — `lx_fsm_arterial_ped_compatible_locked()`).
- **Các bước**:
  1. Trong lúc `CONNECTOR_GREEN`/`CONNECTOR_YELLOW`/`ALL_RED_B_TO_A`, bấm `1` (side 0).
  2. Quan sát: không có dòng `PED SIGNAL side 0 -> WALK` nào xuất hiện ngay.
  3. Chờ tới khi L1 in `SIGNAL -> ARTERIAL GREEN`.
- **Kết quả mong đợi**: Ngay trong cùng tick L1 vào `PHASE_ARTERIAL_GREEN` (thực chất trong tick kế tiếp của `lx_fsm_ped_service_tick_locked()`, vì hàm này đọc `fsm->phase` hiện tại), log in `Lx 1: PED SIGNAL side 0 -> WALK`. Yêu cầu không hề bị huỷ trong lúc chờ (đúng PA-02/ghi chú "not discarded").

### TC-SC02-2: Thời lượng WALK 6000ms và FLASHING_DONT_WALK 4000ms chính xác, latch được xoá đúng lúc
- **Loại**: Positive
- **Liên quan**: SC-02, `WALK --> FLASHING_DONT_WALK : after 6s`, `FLASHING_DONT_WALK --> DONT_WALK : after 4s`
- **Môi trường**: (A)
- **Chuẩn bị**: Tiếp nối TC-SC02-1 hoặc bấm `1` lúc vừa vào ARTERIAL_GREEN.
- **Các bước**: Đo thời gian giữa 3 dòng log: `WALK` → `FLASHING_DONT_WALK` → `DONT_WALK`.
- **Kết quả mong đợi**: `WALK` (t=0) → `FLASHING_DONT_WALK` (t≈6.0s) → `DONT_WALK` (t≈10.0s). Sau dòng `DONT_WALK`, nếu bấm lại `1`, một chuỗi WALK/FDW **mới** phải khởi động lại từ đầu khi ARTERIAL_GREEN kế tiếp tới (xác nhận `ped_latched[0]` đã bị xoá về 0 ở `lx_fsm_ped_service_tick_locked()`, nhánh `else { fsm->ped_latched[side] = 0; }`).

### TC-SC02-3: Coalesce nhiều lần bấm khi còn ở REQUEST_LATCHED
- **Loại**: Positive
- **Liên quan**: SC-02, `REQUEST_LATCHED --> REQUEST_LATCHED : PED_REQUEST(side) [request already pending]`
- **Môi trường**: (A)
- **Chuẩn bị**: L1 đang `CONNECTOR_GREEN` (side 0 chưa tương thích).
- **Các bước**: Bấm `1` liên tục 5 lần cách nhau ~1s trong lúc vẫn ở CONNECTOR_GREEN/YELLOW/ALL_RED.
- **Kết quả mong đợi**: Không có log nào phụ sinh ra từ các lần bấm thừa (setter `lx_fsm_latch_pedestrian_request()` chỉ set `ped_latched[0]=1`, việc set lại nhiều lần là vô hại/idempotent). Khi ARTERIAL_GREEN tới, chỉ **một** chuỗi WALK/FDW chạy cho side 0, không lặp lại hay kéo dài hơn bình thường.

### TC-SC02-4: Coalesce khi bấm lại trong lúc đang WALK (không tự làm mới thời lượng)
- **Loại**: Positive
- **Liên quan**: SC-02, `WALK --> WALK : PED_REQUEST(side) / coalesce repeated request`
- **Môi trường**: (A)
- **Chuẩn bị**: side 0 đang trong WALK (t≈2s kể từ khi WALK bắt đầu).
- **Các bước**: Bấm `1` lại ở t≈2s (giữa khoảng 0–6s của WALK).
- **Kết quả mong đợi**: `FLASHING_DONT_WALK` vẫn xuất hiện đúng ở t≈6.0s tính từ lúc WALK **ban đầu** bắt đầu (không bị dời ra t≈8s) — xác nhận việc bấm giữa chừng không reset `ped_phase_elapsed_ms`. (Về mặt code, bấm khi đang WALK rơi vào nhánh `ped_recall[side]=1` vì `ped_serving_mask` đã có bit side 0 — xem TC-SC02-5 để kiểm tra hệ quả của cờ này.)

### TC-SC02-5 (Critical regression): `ped_recall` — bấm lại giữa chừng không làm mất yêu cầu
- **Loại**: Positive / Regression
- **Liên quan**: SC-02 note "remains latched, not discarded (PA-02)" + cơ chế `ped_recall` trong `lx_fsm_ped_service_tick_locked()`/`lx_fsm_latch_pedestrian_request()`
- **Môi trường**: (A)
- **Chuẩn bị**: side 0 đang được phục vụ (WALK hoặc FDW đang chạy, `ped_serving_mask` có bit 0).
- **Các bước**:
  1. Trong lúc FLASHING_DONT_WALK đang chạy (ví dụ t≈2s trong cửa sổ 4s của FDW), bấm lại `1`.
  2. Chờ FDW kết thúc → `DONT_WALK` xuất hiện (t≈4s sau bước 1's mốc bắt đầu FDW).
  3. Tiếp tục theo dõi L1 hết vòng CONNECTOR_GREEN/…/ALL_RED_B_TO_A, cho tới khi ARTERIAL_GREEN quay lại.
- **Kết quả mong đợi**: Ngay sau khi `DONT_WALK` được in ra, `ped_latched[0]` **vẫn còn là 1** (không bị xoá) vì nhánh `if (fsm->ped_recall[side])` trong code chỉ xoá `ped_recall[side]` về 0, cố ý **không** chạm `ped_latched[side]`. Do đó ngay tại lần ARTERIAL_GREEN kế tiếp, một chuỗi WALK/FDW **mới** cho side 0 phải tự khởi động lại mà không cần bấm nút `1` thêm lần nào — đây là bằng chứng trực tiếp yêu cầu bấm giữa chừng không bị rơi mất.

### TC-SC02-6: Bấm nút khác (chưa nằm trong compatible_mask hiện tại) trong lúc đang phục vụ side khác cùng pha
- **Loại**: Edge case
- **Liên quan**: SC-02, comment "A side that latches mid-sequence for the SAME phase is picked up the next time a sequence starts ... not folded into one already in progress"
- **Môi trường**: (A)
- **Chuẩn bị**: side 0 đang WALK (side 1 **chưa** được bấm khi WALK bắt đầu, tức `compatible_mask` lúc khởi động chỉ có bit 0).
- **Các bước**:
  1. t≈2s trong WALK của side 0, bấm `2` (side 1 — cũng tương thích ARTERIAL_GREEN nhưng đến muộn).
  2. Quan sát: không có `PED SIGNAL side 1 -> WALK` xuất hiện ngay (vì `ped_serving_mask` hiện tại chỉ có bit 0, side 1 không được gộp vào).
  3. Theo dõi hết chuỗi side 0 (WALK→FDW→DONT_WALK) rồi hết vòng CONNECTOR/ALL_RED, tới khi ARTERIAL_GREEN tiếp theo bắt đầu.
- **Kết quả mong đợi**: side 1 chỉ bắt đầu WALK ở lần ARTERIAL_GREEN **kế tiếp** (một chu kỳ 90s sau, trong PEAK_FIXED), không được gộp chung/rút ngắn vào chuỗi của side 0 đang chạy — khớp đúng thiết kế "mỗi lần chỉ có một instance chuỗi WALK/FDW."

---

## 5. SC-03A — Intersection Supervisory Authority Overview

### TC-SC03A-1 (CRITICAL REGRESSION): RAILWAY_PREEMPTION không làm kẹt cả giao lộ ở ALL_RED — arterial vẫn chạy vòng lặp bình thường, chỉ connector bị suppress
- **Loại**: Positive / Regression (đây là bug quan trọng nhất đã fix — trước đây một `break` trần trong `lx_fsm_advance_phase_locked()` khiến `fsm->phase` bị đứng vĩnh viễn ở `PHASE_ALL_RED_A_TO_B`)
- **Liên quan**: SC-03A, `NORMAL_OPERATION --> RAILWAY_PREEMPTION` và hành vi bên trong (CC-02)
- **Môi trường**: (B) hoặc (C) — cần `rlx_main 1` (RL1, kề L1/L2) + `lx_main 1`. Không bắt buộc `c_main` (crossing status đi thẳng RLx→Lx qua `rlx_comm_broadcast_crossing_status_if_changed()`), nhưng nên chạy kèm `c_main` để tiện quan sát bảng SUPERVISORY qua `c_hmi_render()`.
- **Chuẩn bị**: L1 ở `MODE_PEAK_FIXED`, đang chạy vòng lặp bình thường. RL1 ở `RLX_OPEN`.
- **Các bước**:
  1. Trên console RL1, bấm `0` (TRAIN_APPROACHING hướng 0) → RL1 vào `RLX_WARNING`, gửi `MSG_CROSSING_STATUS(WARNING)` tới L1 và L2 gần như ngay lập tức (broadcast mỗi tick 1s nếu có thay đổi).
  2. Trên C1 (nếu chạy), quan sát cột SUPERVISORY của L1 chuyển từ `3` (NORMAL_OPERATION) sang `1` (RAILWAY_PREEMPTION) trong vòng ≤1s.
  3. **Theo dõi log của L1 liên tục trong ít nhất 3 phút** (đủ để RL1 tự nhiên đi qua WARNING(5s)→CLOSING(~3s)→CLOSED→chờ 20s→TRAIN_PRESENT→chờ 20s→OPENING(~3s), tức khoảng 51s tối thiểu nếu không có train thứ hai — nhưng vì ta **không** cho crossing mở lại ở bước này, chỉ cần theo dõi qua giai đoạn CLOSED kéo dài).
  4. Đếm số lần `Lx 1: SIGNAL -> ARTERIAL GREEN` xuất hiện trong khoảng thời gian RAILWAY_PREEMPTION đang active (RL1 chưa OPEN lại).
- **Kết quả mong đợi**:
  - L1 phải hiện **nhiều hơn một** chu kỳ đầy đủ `ARTERIAL_GREEN → ARTERIAL_YELLOW → ALL_RED_A_TO_B → ARTERIAL_GREEN` trong lúc RAILWAY_PREEMPTION còn active — nghĩa là arterial tiếp tục chạy đúng nhịp 48+4+2=54s/vòng (không có 90s connector xen giữa).
  - **Không bao giờ** xuất hiện dòng `Lx 1: SIGNAL -> CONNECTOR GREEN` trong suốt thời gian RAILWAY_PREEMPTION active — mọi lần tới `PHASE_ALL_RED_A_TO_B` phải quay thẳng lại `PHASE_ARTERIAL_GREEN` (nhánh `if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) { fsm->phase = PHASE_ARTERIAL_GREEN; break; }` trong `lx_fsm_advance_phase_locked()`).
  - Đặc biệt: L1 **không bao giờ đứng yên** ở `ALL RED (A to B)` lâu hơn 2s một lần nào — nếu log cho thấy L1 kẹt ở `ALL RED (A to B)` mãi không đổi trong khi RL1 vẫn ở WARNING/CLOSED, đây chính là bug cũ đã được fix tái xuất hiện (regression thật sự).

### TC-SC03A-2: NORMAL_OPERATION → CENTRAL_OVERRIDE (chấp nhận REQUEST_OVERRIDE)
- **Loại**: Positive
- **Liên quan**: SC-03A, `NORMAL_OPERATION --> CENTRAL_OVERRIDE : REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)`
- **Môi trường**: (B)
- **Chuẩn bị**: L1 NORMAL_OPERATION, không có pedestrian clearance đang chạy (`ped_clearance_active=0` — tránh bấm nút đi bộ trước đó).
- **Các bước**: Trên C1 bấm `o`, Lx=`1`, target movement=`0` (arterial), duration_ms=`20000`.
- **Kết quả mong đợi**: `c_operator` log "REQUEST_OVERRIDE(...) submitted"; SUPERVISORY của L1 trên C1 chuyển sang `2` (CENTRAL_OVERRIDE), cột OVERRIDE=`1` trong vòng ≤1s.

### TC-SC03A-3: CENTRAL_OVERRIDE → RAILWAY_PREEMPTION khi crossing kề bên trở nên active giữa lúc override đang chạy
- **Loại**: Positive (đường vào tranh chấp: override active + train đến gần đồng thời)
- **Liên quan**: SC-03A, `CENTRAL_OVERRIDE --> RAILWAY_PREEMPTION : adjacent crossing becomes active / cancel or terminate override, suppress toward-crossing movement`
- **Môi trường**: (B), cần cả `rlx_main 1` và `lx_main 1` và `c_main`.
- **Chuẩn bị**: Thực hiện TC-SC03A-2 trước để L1 đang CENTRAL_OVERRIDE (target=connector, duration dài, ví dụ 60000ms để có thời gian thao tác).
- **Các bước**:
  1. Trong lúc override còn active (ví dụ ở giây thứ 10/60), trên RL1 bấm `0`.
  2. Quan sát log L1: phải có `Lx 1: override cleared/expired - running safe clearance sequence` (từ `lx_signal_show_override_clearance()`, gọi trong `lx_fsm_terminate_override_locked()`) xuất hiện **trước hoặc cùng lúc** SUPERVISORY chuyển sang RAILWAY_PREEMPTION.
- **Kết quả mong đợi**: SUPERVISORY L1 trên C1: `2` → `1` (không đi qua `3` trung gian). OVERRIDE về `0`. Sau đó hành vi giống hệt TC-SC03A-1 (arterial tiếp tục chạy, connector bị suppress) — override **không tự động resume** sau khi crossing mở lại (đúng ghi chú SC-03A "an override interrupted by a train is never automatically resumed afterward").

### TC-SC03A-4: RAILWAY_PREEMPTION self-loop — REQUEST_OVERRIDE xung đột bị NACK
- **Loại**: Negative
- **Liên quan**: SC-03A, `RAILWAY_PREEMPTION --> RAILWAY_PREEMPTION : conflicting REQUEST_OVERRIDE(CLEAR_ROUTE) / NACK`
- **Môi trường**: (B)
- **Chuẩn bị**: L1 đang RAILWAY_PREEMPTION (RL1 ở WARNING/CLOSING/CLOSED).
- **Các bước**: Trên C1 bấm `o`, Lx=`1`, target=`1` (connector), duration=`10000`.
- **Kết quả mong đợi**: `lx_fsm_on_request_override()` trả `RESULT_NACK`, `reply->reason = NACK_REASON_RAILWAY_CONFLICT` — log ở console C1 hiển thị NACK (qua `c_comm.c`'s reply logging). SUPERVISORY L1 không đổi, vẫn `1`.

### TC-SC03A-5: RAILWAY_PREEMPTION → NORMAL_OPERATION khi crossing OPEN lại, không có queue-warning → không có drain
- **Loại**: Positive
- **Liên quan**: SC-03A, `RAILWAY_PREEMPTION --> NORMAL_OPERATION : crossing reports OPEN and connector drain completes`; UC-05 alt 6.1 "no warning -> skip drain"
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: L1 đang RAILWAY_PREEMPTION do RL1. Đảm bảo **không** bấm `w` trên L1 trước đó (`queue_warning_active=0`).
- **Các bước**: Không tương tác gì thêm, chờ RL1 tự nhiên đi hết `WARNING→CLOSING→CLOSED→TRAIN_PRESENT→OPENING→OPEN` (~51s không có train thứ hai) → RL1 broadcast `CROSSING_STATUS(OPEN)`.
- **Kết quả mong đợi**: Ngay khi L1 nhận `CROSSING_OPEN`, SUPERVISORY chuyển `1 → 3` (NORMAL_OPERATION) trong `lx_fsm_on_crossing_status()`. `drain_pending` **không** được set (vì `fsm->queue_warning_active==0`) — lần `CONNECTOR_GREEN` kế tiếp phải có thời lượng bình thường (30s cho PEAK_FIXED), không kéo dài thêm — xác nhận không có log gia hạn nào và `CONNECTOR_YELLOW` xuất hiện đúng 30s sau `CONNECTOR_GREEN`.

### TC-SC03A-6: NORMAL_OPERATION/CENTRAL_OVERRIDE → FAULT_SAFE qua watchdog; và lỗ hổng đã biết khi thoát FAULT_SAFE
- **Loại**: Positive (vào FAULT_SAFE) + Negative/Known gap (ra khỏi FAULT_SAFE)
- **Liên quan**: SC-03A, `NORMAL_OPERATION --> FAULT_SAFE`, `CENTRAL_OVERRIDE --> FAULT_SAFE`, `FAULT_SAFE --> NORMAL_OPERATION : verified repair and accepted local fault-clear request`
- **Môi trường**: (B)
- **Chuẩn bị/Các bước — Phần 1 (từ CENTRAL_OVERRIDE)**:
  1. Đưa L1 vào CENTRAL_OVERRIDE (như TC-SC03A-2).
  2. Tạm dừng tiến trình `lx_main 1` bằng `kill -STOP <pid>` trong >2s rồi `kill -CONT <pid>` để kích watchdog thật (`lx_watchdog_thread`).
  3. Quan sát log `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)`, sau đó `Lx 1: override cleared/expired - running safe clearance sequence` (override bị terminate **trước** khi vào FAULT_SAFE — đây là compliance-audit fix trong `lx_fsm_report_watchdog_trip()`/`lx_fsm_check_fault_locked()`), rồi `Lx 1: FAULT_SAFE - holding safe outputs (all-red/dark)`.
- **Kết quả mong đợi Phần 1**: SUPERVISORY L1: `2 → 0` trực tiếp (không qua `3`), OVERRIDE về `0`.
- **Các bước — Phần 2 (thử khôi phục)**: Không có phím/console nào trên `lx_main`/`c_operator` gọi tới `lx_fsm_local_fault_clear()` — hàm này tồn tại trong `lx_fsm.c`/`lx_fsm.h` nhưng **không được gọi ở bất kỳ đâu** trong `lx_main.c`/`lx_sensor.c` (khác hẳn RLx có phím `f` và `MSG_REQUEST_FAULT_CLEAR`). Thử mọi phím trên `lx_sensor.c` (`a A c C 1 2 3 4 w W`) và mọi lệnh operator liên quan tới L1.
- **Kết quả mong đợi Phần 2 (Known gap)**: L1 **vẫn ở FAULT_SAFE vô thời hạn** — không có cách nào qua giao diện hiện có (bàn phím hay IPC) để đưa L1 trở lại NORMAL_OPERATION. Đây là một lỗ hổng chức năng đã biết, cần ghi vào biên bản test là **FAIL/GAP** cho riêng nhánh `FAULT_SAFE -> NORMAL_OPERATION` của Lx (khác với RLx, xem SC-04B TC-SC04B-4 — RLx có đường hồi phục hoạt động qua `MSG_REQUEST_FAULT_CLEAR`/phím `f`). Khuyến nghị: thêm một verb `MSG_REQUEST_FAULT_CLEAR` cho Lx hoặc nối `lx_fsm_local_fault_clear()` vào một phím demo tương tự RLx trước khi coi UC liên quan là hoàn thành.

### TC-SC03A-7 (Critical regression, CC-03): Drain-phase chỉ được cấp đúng một lần cho mỗi lần railway mở lại
- **Loại**: Positive / Regression
- **Liên quan**: SC-03A (edge RAILWAY_PREEMPTION→NORMAL_OPERATION) kết hợp CC-03, hiện thực tại `lx_fsm_on_crossing_status()` (set `drain_pending`) và `lx_fsm_advance_phase_locked()`/`lx_fsm_on_phase_timer()` (tiêu thụ `drain_pending`, chạy `drain_active`/`drain_extending`)
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: L1 `MODE_PEAK_FIXED`, RL1 kề L1. Trên L1 bấm `w` (bật `queue_warning_active=1`) **trong lúc** RL1 đang ở WARNING/CLOSED (RAILWAY_PREEMPTION đang active ở L1).
- **Các bước**:
  1. Với `queue_warning_active=1`, chờ RL1 tự mở lại (`CROSSING_OPEN`) → L1 nhận, `drain_pending=1` được set trong `lx_fsm_on_crossing_status()`.
  2. Theo dõi lần `CONNECTOR_GREEN` **đầu tiên** sau đó: nó phải đạt hết 30s bình thường (`LX_PEAK_CONNECTOR_GREEN_MS`), sau đó — vì `drain_active=1` — thay vì chuyển `CONNECTOR_YELLOW` ngay, nó phải gia hạn thêm từng nấc 4s (`LX_EXTENSION_MS`) **miễn là** `queue_warning_active` vẫn còn 1.
  3. **Không** bấm lại `W` (tắt queue warning) — để nó tự chạy tới trần.
  4. Đo tổng thời gian gia hạn: phải dừng đúng khi đạt `LX_DRAIN_MAX_EXTENSION_MS`=60000ms (tức 15 nấc x 4s), sau đó `CONNECTOR_GREEN -> CONNECTOR_YELLOW` xảy ra dù `queue_warning_active` vẫn =1 (nhánh `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` thắng bất kể warning còn hay không).
  5. Sau khi hết drain, để L1 chạy hết vòng tiếp theo và (nếu muốn) lặp lại một pha CONNECTOR_GREEN **thứ hai** mà **không** có thêm lần RAILWAY_PREEMPTION→NORMAL_OPERATION mới nào xảy ra.
- **Kết quả mong đợi**:
  - Tổng thời gian CONNECTOR_GREEN của lần drain = 30s (base) + tối đa 60s (drain) = tối đa 90s, tăng đúng từng nấc 4s.
  - Lần CONNECTOR_GREEN **kế tiếp sau đó** (bước 5, khi không có RAILWAY_PREEMPTION mới xen giữa) phải trở lại đúng 30s bình thường — **không** được gia hạn lần nữa, vì `drain_pending` chỉ được set đúng một lần tại cạnh chuyển `RAILWAY_PREEMPTION -> NORMAL_OPERATION` (không set lại trong lúc NORMAL_OPERATION bình thường dù `queue_warning_active` vẫn còn 1) — đây chính là điều kiện "đúng 1 lần duy nhất sau mỗi lần railway mở lại" cần xác nhận.
  - Biến thể bổ sung (làm thêm nếu có thời gian): lặp lại toàn bộ test nhưng bấm `W` (tắt warning) ở giữa chừng drain (ví dụ sau 12s gia hạn) — kỳ vọng `CONNECTOR_GREEN -> CONNECTOR_YELLOW` xảy ra ngay ở nấc 4s kế tiếp sau khi tắt (không chờ đủ 60s), khớp nhánh `!fsm->queue_warning_active`.

---

## 6. SC-03B — Clear-Route Override Validation, Pending, and Renewal Detail

### TC-SC03B-1: override_validation → ACTIVE ngay lập tức (bounded, safe, không có pedestrian clearance)
- **Loại**: Positive
- **Liên quan**: SC-03B, `override_validation --> ACTIVE : [bounded, safe, and no pedestrian clearance active] / ACK, apply at safe boundary`
- **Môi trường**: (B)
- **Chuẩn bị**: L1 NORMAL_OPERATION, không có `ped_clearance_active`.
- **Các bước**: Bấm `o`, Lx=`1`, target=`0` (arterial), duration=`15000`.
- **Kết quả mong đợi**: `lx_fsm_on_request_override()` trả `RESULT_ACK` (không phải `ACK_PENDING`) ngay lập tức; `override_substate` = `OVR_ACTIVE`; nếu L1 hiện đang ở `PHASE_CONNECTOR_GREEN`, phase timer giữ nguyên chờ tới boundary `ALL_RED_B_TO_A`/`ALL_RED_A_TO_B` gần nhất rồi mới ép về đúng `PHASE_ARTERIAL_GREEN` (không cắt ngang phase đang chạy — "apply at safe boundary").

### TC-SC03B-2: override_validation → OVERRIDE_PENDING khi pedestrian clearance đang chạy, rồi tự kích hoạt khi clearance xong
- **Loại**: Positive (đường vào có điều kiện tranh chấp: override request đến đúng lúc ped clearance đang chạy)
- **Liên quan**: SC-03B, `override_validation --> OVERRIDE_PENDING` rồi `OVERRIDE_PENDING --> ACTIVE : pedestrian clearance completes [request remains safe and valid]`
- **Môi trường**: (B)
- **Chuẩn bị**: Bấm nút đi bộ tương thích với phase hiện tại (ví dụ `1` lúc L1 đang ARTERIAL_GREEN) để có WALK đang chạy (`ped_clearance_active=1`).
- **Các bước**:
  1. Trong lúc WALK/FDW đang chạy (t≈2s trong WALK 6s), bấm `o`, Lx=`1`, target=`0`, duration=`20000`.
  2. Quan sát reply: phải là `RESULT_ACK_PENDING` (không phải `ACK`/`NACK`).
  3. Chờ tới khi `DONT_WALK` xuất hiện (đánh dấu `ped_clearance_active` về 0).
- **Kết quả mong đợi**: Ngay tick kế tiếp sau khi `DONT_WALK` xuất hiện, `override_substate` chuyển `OVR_PENDING_CLEARANCE → OVR_ACTIVE` (nhánh trong `lx_fsm_on_phase_timer()`, chạy **sau** `lx_fsm_ped_service_tick_locked()` trong cùng tick — đây là verifier-audit fix về thứ tự gọi hàm). Không có reply thứ hai nào được gửi thêm cho C1 (ACK ban đầu đã bao trùm, đúng ghi chú SC-03B "not repeated when the queued request later activates"). OVERRIDE trên C1 chuyển `0 → 1`.

### TC-SC03B-3: override_validation → NACK khi duration không hợp lệ
- **Loại**: Negative
- **Liên quan**: SC-03B, `override_validation --> [*] : [unsafe, unbounded, or conflicts with railway pre-emption] / NACK`
- **Môi trường**: (B)
- **Chuẩn bị**: L1 NORMAL_OPERATION.
- **Các bước**:
  1. Bấm `o`, Lx=`1`, target=`0`, duration=`0`.
  2. Bấm `o`, Lx=`1`, target=`0`, duration=`300001`.
- **Kết quả mong đợi**: Cả hai lần đều bị từ chối **ở tầng Central** (`c_mode_eng_validate_override_request()` trong `c_operator.c`, kiểm tra `duration_ms==0 || >300000` — trùng chính xác điều kiện `lx_fsm_on_request_override()` sẽ áp dụng nếu request lọt tới Lx) — console C1 in "rejected by Central pre-check, reason=INVALID_DURATION", request **không** được forward sang L1 (log `c_comm_send_request_override()` không chạy). Đây là phòng thủ hai lớp (defense-in-depth): giới hạn giống hệt cũng tồn tại độc lập trong `lx_fsm_on_request_override()` (`payload->duration_ms == 0 || > LX_OVERRIDE_DURATION_CAP_MS`), nhưng qua console operator thật, lớp Central luôn chặn trước.

### TC-SC03B-4: OVERRIDE_PENDING bị discard khi hết hạn ngay trong lúc còn đang chờ (duration ngắn hơn thời gian chờ ped clearance)
- **Loại**: Edge case (2 điều kiện gần như đồng thời: countdown hết hạn vs. clearance vẫn đang chạy)
- **Liên quan**: SC-03B, `OVERRIDE_PENDING --> [*] : request no longer valid, cancelled, or expires before application / discard request`
- **Môi trường**: (B)
- **Chuẩn bị**: Kích hoạt một chuỗi ped clearance dài nhất có thể (WALK 6s + FDW 4s = 10s) đúng lúc phase hiện tại — bấm nút đi bộ tương thích ngay khi vừa vào ARTERIAL_GREEN.
- **Các bước**:
  1. Ngay sau khi WALK bắt đầu (t≈0.5s), gửi `o`, Lx=`1`, target=`0`, duration=`3000` (ngắn hơn nhiều so với ~9.5s còn lại của chuỗi ped).
  2. Quan sát reply: `RESULT_ACK_PENDING`.
  3. Theo dõi `override_remaining_ms` gián tiếp qua log: vì `lx_fsm_on_phase_timer()` vẫn đếm ngược `override_remaining_ms` trong cả `OVR_ACTIVE` **và** `OVR_PENDING_CLEARANCE` (compliance-audit fix), countdown 3000ms sẽ chạm 0 ở t≈3.0s — **trước khi** `DONT_WALK` xuất hiện (t≈10.0s).
- **Kết quả mong đợi**: Ở t≈3.0s, `lx_fsm_terminate_override_locked()` được gọi ngay cả khi vẫn đang `OVR_PENDING_CLEARANCE` — log `Lx 1: override cleared/expired - running safe clearance sequence` xuất hiện, `supervisory` trở về `NORMAL_OPERATION`, OVERRIDE trên C1 về `0` — **trong khi WALK/FDW của người đi bộ vẫn tiếp tục chạy không bị gián đoạn** (đúng nguyên tắc "pedestrian sequence itself is never truncated"). Không có reply thứ hai gửi cho C1.

### TC-SC03B-5: renewal_validation — cả hai nhánh hợp lệ/không hợp lệ, và NACK khi không có override active để renew
- **Loại**: Positive + Negative (kết hợp)
- **Liên quan**: SC-03B, `renewal_validation --> ACTIVE : [bounded and safe] / ACK, restart override timer` và `[invalid, unsafe, or over limit] / NACK, retain current expiry`
- **Môi trường**: (B)
- **Chuẩn bị — nhánh (a) hợp lệ**: L1 đang `OVR_ACTIVE` với `duration_ms=20000`, đã trôi qua ~10s (`override_remaining_ms≈10000`).
- **Các bước (a)**: Bấm `r`, Lx=`1`, extend_duration_ms=`30000`.
- **Kết quả mong đợi (a)**: `RESULT_ACK`; `override_duration_ms` và `override_remaining_ms` đều được set lại thành `30000` (restart hoàn toàn, không cộng dồn) — override giờ sẽ hết hạn 30s kể từ **thời điểm renew**, không phải từ lúc bắt đầu.
- **Các bước (b) — nhánh không hợp lệ**: Ngay sau (a), bấm `r`, Lx=`1`, extend_duration_ms=`400000` (>300000).
- **Kết quả mong đợi (b)**: `RESULT_NACK`, `NACK_REASON_INVALID_DURATION`; `override_remaining_ms` **giữ nguyên** giá trị đang đếm ngược từ (a) (không bị reset về 0 hay bị sửa) — đúng "retain current expiry".
- **Các bước (c) — không có override để renew**: Trên một Lx khác chưa từng có override (ví dụ L2), bấm `r`, Lx=`2`, extend_duration_ms=`10000`.
- **Kết quả mong đợi (c)**: `lx_fsm_on_renew_override()` kiểm tra `fsm->supervisory != SUPERVISORY_CENTRAL_OVERRIDE || fsm->override_substate != OVR_ACTIVE` → `RESULT_NACK`, `NACK_REASON_UNKNOWN_TARGET`. (Renew một override đang `OVR_PENDING_CLEARANCE` cũng phải NACK cùng lý do — có thể test thêm bằng cách renew ngay trong kịch bản TC-SC03B-2 trước khi clearance xong.)

### TC-SC03B-6 (Critical regression): override ép đèn xanh thật đúng hướng, giữ tới hết thời lượng, rồi trả về chu kỳ bình thường qua đúng vàng/đỏ
- **Loại**: Positive / Regression
- **Liên quan**: SC-03B "ACTIVE" note ("Bounded and auto-expiring") kết hợp SC-01B/C — hiện thực tại các nhánh `SUPERVISORY_CENTRAL_OVERRIDE && OVR_ACTIVE` trong `lx_fsm_on_phase_timer()` (case `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN`, giữ green) và `lx_fsm_advance_phase_locked()` (ép chọn đúng `override_target_movement` tại hai boundary `ALL_RED_A_TO_B`/`ALL_RED_B_TO_A`)
- **Môi trường**: (B)
- **Chuẩn bị**: Đợi L1 đang ở `PHASE_CONNECTOR_GREEN` (tức là hướng "sai" so với override sắp gửi).
- **Các bước**:
  1. Trong lúc `CONNECTOR_GREEN`, bấm `o`, Lx=`1`, target=`0` (**arterial** — ngược hướng đang chạy), duration=`15000`.
  2. Theo dõi: `CONNECTOR_GREEN` phải tiếp tục chạy hết bình thường tới `CONNECTOR_YELLOW` (4s) → `ALL_RED_B_TO_A` (2s) — override **không** cắt ngang phase/clearance đang hiển thị.
  3. Tại ranh giới `ALL_RED_B_TO_A` kết thúc, quan sát phase kế tiếp.
  4. Đếm thời gian L1 giữ nguyên `ARTERIAL_GREEN` liên tục.
  5. Sau 15s kể từ lúc override được ACK (không phải từ lúc ARTERIAL_GREEN bắt đầu — `override_remaining_ms` đếm từ lúc ACK), quan sát điều gì xảy ra.
- **Kết quả mong đợi**:
  - Bước 3: `lx_fsm_advance_phase_locked()` case `PHASE_ALL_RED_B_TO_A` phải chọn `PHASE_ARTERIAL_GREEN` — đây **luôn đúng** ở cả hai trường hợp override lẫn bình thường ở boundary này (không có gì để phân biệt tại đây), nên test chính nằm ở boundary `PHASE_ALL_RED_A_TO_B` sau đó.
  - `ARTERIAL_GREEN` không tự thoát ở 48s như PEAK_FIXED thường lệ — nhánh `if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && ... && override_target_movement == OVERRIDE_MOVEMENT_ARTERIAL) { break; }` bỏ qua hoàn toàn exit-check bình thường, giữ ARTERIAL_GREEN **cho tới khi** `override_remaining_ms` chạm 0.
  - Đúng tại thời điểm 15s kể từ ACK, log `Lx 1: override cleared/expired - running safe clearance sequence` xuất hiện, SUPERVISORY về `3`, và **ngay sau đó** phase vẫn đang là `ARTERIAL_GREEN` (vì lx_fsm không tự ép đổi phase khi override kết thúc — chỉ đổi supervisory) — nó sẽ tiếp tục chạy exit-check PEAK_FIXED bình thường (tới 48s tổng cộng kể từ khi ARTERIAL_GREEN bắt đầu, **không phải** kể từ lúc override hết hạn) rồi mới sang `ARTERIAL_YELLOW → ALL_RED_A_TO_B → CONNECTOR_GREEN` như chu kỳ bình thường — tức luôn đi qua đúng vàng/đỏ, không nhảy thẳng.
  - Biến thể bổ sung (edge case bắt buộc): lặp lại toàn bộ test nhưng gửi hai `REQUEST_OVERRIDE` liên tiếp cho cùng L1 mà **không** cancel/hết hạn cái đầu — lần thứ hai phải bị `RESULT_NACK`/`NACK_REASON_OUT_OF_RANGE` (nhánh `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE` ở đầu `lx_fsm_on_request_override()` — compliance-audit fix ngăn override thứ hai âm thầm ghi đè target/duration của override thứ nhất).

---

## 7. SC-04A — Railway Crossing Approach and Closure

### TC-SC04A-1: OPEN → WARNING khi tàu tiếp cận
- **Loại**: Positive
- **Liên quan**: SC-04A, `OPEN --> WARNING : TRAIN_APPROACHING(direction) / activate flashers, notify Lx and C1, register occupancy window`
- **Môi trường**: (A) `rlx_main 1` đơn lẻ là đủ để thấy log nội bộ; dùng (B) nếu muốn xác nhận notify Lx/C1.
- **Chuẩn bị**: RL1 ở `RLX_OPEN`.
- **Các bước**: Bấm `0`.
- **Kết quả mong đợi**: Log `RLx: flashers ON (train approaching, direction 0)`. Nếu chạy (B): L1 và L2 nhận `MSG_CROSSING_STATUS(WARNING)`; nếu `c_main` chạy, CROSSING_STATE của RL1 trên bảng C1 chuyển `0 → 1`.

### TC-SC04A-2: WARNING self-loop — hướng thứ hai tiếp cận trong lúc đang WARNING
- **Loại**: Positive
- **Liên quan**: SC-04A, `WARNING --> WARNING : TRAIN_APPROACHING(other direction) / register additional occupancy window`
- **Môi trường**: (A)
- **Chuẩn bị**: RL1 vừa vào WARNING do bấm `0` (t=0).
- **Các bước**: Ở t≈2s, bấm `1`.
- **Kết quả mong đợi**: `register_window()` tạo cửa sổ thứ hai (direction 1) mà **không** reset `state_elapsed_ms` (biến đếm tới `RLX_WARNING_TO_CLOSING_MS` vẫn tính từ t=0, không phải từ t=2s) — `CLOSING` vẫn phải bắt đầu đúng ở t≈5.0s (không bị dời sang t≈7.0s).

### TC-SC04A-3: WARNING → CLOSING đúng biên 5000ms
- **Loại**: Edge case
- **Liên quan**: SC-04A, `WARNING --> CLOSING : after 5 s / command both gates down`
- **Môi trường**: (A)
- **Chuẩn bị**: RL1 OPEN.
- **Các bước**: Bấm `0`, đo thời gian tới khi log `RLx: commanding gates DOWN (simulated motion, 3000 ms)` xuất hiện.
- **Kết quả mong đợi**: Hiệu số ≈5.0s (±1 tick 1000ms, vì railway tick chu kỳ 1s chứ không phải 100ms như Lx). *Ghi chú known-gap:* transition `WARNING --> FAULT : approach input remains active beyond diagnostic timeout` (60s, `RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS`) **không thể xảy ra** trong cài đặt hiện tại — comment trong `rlx_fsm_on_tick()` xác nhận đây là dead code vì `RLX_WARNING_TO_CLOSING_MS`(5s) luôn bắn trước và reset `state_elapsed_ms` mỗi khi `enter_closing()` chạy; sự kiện `TRAIN_APPROACHING` mô phỏng rời rạc cũng không thể hiện được một sensor "kẹt active liên tục" thật. Không cần (và không thể) viết test case dương cho nhánh này — chỉ xác nhận WARNING luôn đi CLOSING ở 5s như trên là đủ để coi là "negative control" cho nhánh FAULT này.

### TC-SC04A-4: CLOSING → CLOSED khi cả hai cổng xác nhận đóng trước deadline
- **Loại**: Positive
- **Liên quan**: SC-04A, `gate_confirmation --> CLOSED : [both gates confirmed CLOSED before deadline] / set train signal(s) for registered approaches to PROCEED`
- **Môi trường**: (A)
- **Chuẩn bị**: RL1 OPEN, không bấm `x` (không arm demo fault).
- **Các bước**: Bấm `0`, chờ qua WARNING(5s)→CLOSING.
- **Kết quả mong đợi**: Ở t≈5+3=8.0s (5s warning + `RLX_GATE_MOTION_MS`=3000ms mô phỏng), log lần lượt: `RLx: commanding gates DOWN...` (t≈5.0s) rồi `RLx: train signal PROCEED for direction 0 (gates confirmed closed)` (t≈8.0s) — đúng trước deadline 15s (`RLX_CLOSING_DEADLINE_MS`, tính từ lúc vào CLOSING nên deadline thật là t≈20.0s).

### TC-SC04A-5 (Regression): CLOSING → FAULT khi xác nhận cổng thiếu ở deadline — fault ép gate đóng thật
- **Loại**: Negative / Regression
- **Liên quan**: SC-04A, `gate_confirmation --> FAULT : [confirmation missing or contradictory at deadline] / hold STOP and report fault`; `enter_fault()` gọi `rlx_gate_command_close()` (audit fix — trước đây fault chỉ latch cờ, không ép gate đóng thật)
- **Môi trường**: (A)
- **Chuẩn bị**: RL1 OPEN.
- **Các bước**:
  1. Bấm `x` (arm demo fault cho lần motion kế tiếp).
  2. Bấm `0` → vào WARNING → sau 5s vào CLOSING, `rlx_gate_command_close()` chạy (log `commanding gates DOWN`), nhưng do đã arm fault, `rlx_gate_on_tick()` sẽ không set `g_confirmed_closed=1` khi hết 3000ms — thay vào đó log `RLx: gate FAILED TO CONFIRM (simulated fault) ...`.
  3. Chờ tới đúng `RLX_CLOSING_DEADLINE_MS`=15000ms kể từ lúc vào CLOSING (t≈5+15=20.0s kể từ lúc bấm `0`).
- **Kết quả mong đợi**: Đúng ở t≈20.0s, `check_closing_or_reclosing_complete()` thấy `state_elapsed_ms >= 15000` và `gates_confirmed_closed()==0` → gọi `enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)`. Log phải xuất hiện **thêm một lần nữa** `RLx: commanding gates DOWN (simulated motion, 3000 ms)` (do `enter_fault()` gọi lại `rlx_gate_command_close()` một cách vô điều kiện — đây chính là regression cần xác nhận: trước fix, fault không ép lệnh đóng cổng thật, có thể để cổng lơ lửng). Tiếp theo là `RLx: FAULT latched (fault bit 0x1) - holding STOP on all train signals, commanding gates DOWN`. CROSSING_STATE trên C1 (nếu chạy) = `3` (FAULT).

---

## 8. SC-04B — Railway Crossing Occupancy and Reopening

### TC-SC04B-1: CLOSED → TRAIN_PRESENT khi tới giờ dự kiến, gate vẫn xác nhận đóng
- **Loại**: Positive
- **Liên quan**: SC-04B, `CLOSED --> TRAIN_PRESENT : expected arrival time reached [gates remain confirmed CLOSED] / mark occupancy window active`
- **Môi trường**: (A)
- **Chuẩn bị**: Lặp lại TC-SC04A-4 để RL1 vào `RLX_CLOSED` (t≈8.0s kể từ lúc bấm `0`).
- **Các bước**: Không tương tác gì thêm, chờ `RLX_EXPECTED_ARRIVAL_MS`=20000ms kể từ lúc vào CLOSED.
- **Kết quả mong đợi**: Không có log rõ ràng cho transition này (code không in gì ở `enter_train_present()` ngoài việc set state nội bộ), nhưng có thể suy luận gián tiếp: CROSSING_STATE bên ngoài (qua `map_to_crossing_state()`) **vẫn giữ nguyên** `CROSSING_CLOSED` (cả `RLX_CLOSED` và `RLX_TRAIN_PRESENT` đều map về `CROSSING_CLOSED`) — verify bằng cách quan sát cửa sổ occupancy: bấm `0` lần nữa ngay sau mốc 20s (t≈28s) và xác nhận log `RLx: train signal PROCEED for direction 0 ...` xuất hiện lại ngay lập tức (nhánh `RLX_TRAIN_PRESENT` trong `rlx_fsm_simulate_train_approaching()` gọi thẳng `register_window(..., RLX_OCCUPANCY_WINDOW_MS)` rồi in PROCEED ngay, khác với nhánh `RLX_CLOSED` cũng làm vậy — nên để phân biệt chắc chắn cần thêm khoảng chờ và log timing).
  *Ghi chú*: đây là transition khó quan sát trực tiếp qua log — nhóm nên cân nhắc thêm một dòng debug tạm thời khi chạy test này nếu cần bằng chứng dứt khoát hơn là suy luận qua thời gian.

### TC-SC04B-2: TRAIN_PRESENT → OPENING chỉ khi TẤT CẢ cửa sổ occupancy hết hạn (không phải một cửa sổ đơn lẻ)
- **Loại**: Edge case (2 cửa sổ occupancy lệch nhau)
- **Liên quan**: SC-04B, `TRAIN_PRESENT --> OPENING : all occupancy windows expire`; RC-04 invariant trong `rlx_fsm_on_tick()` (`if (fsm->active_window_count == 0)`)
- **Môi trường**: (A)
- **Chuẩn bị**: Đưa RL1 vào TRAIN_PRESENT với **một** cửa sổ hướng 0 đang chạy (theo TC-SC04B-1, cửa sổ hướng 0 bắt đầu đếm 20s kể từ lúc vào TRAIN_PRESENT).
- **Các bước**:
  1. Ngay khi vào TRAIN_PRESENT (t=0 tính từ mốc này), ở t≈10s bấm `1` (đăng ký thêm cửa sổ hướng 1 — vì đang ở `RLX_TRAIN_PRESENT`, cửa sổ này được gán ngay `remaining_ms=RLX_OCCUPANCY_WINDOW_MS`=20000ms, tức sẽ hết hạn ở t≈30s).
  2. Quan sát tại t≈20s (khi cửa sổ hướng 0 hết hạn) — RL1 **không được** vào OPENING.
  3. Tiếp tục quan sát tới t≈30s.
- **Kết quả mong đợi**: Ở t≈20s, `active_window_count` giảm từ 2 xuống 1 (chỉ cửa sổ hướng 0 đóng), state **vẫn là** `RLX_TRAIN_PRESENT` — log `RLx: commanding gates UP...` **không** xuất hiện ở mốc này. Chỉ tới t≈30s khi cửa sổ hướng 1 cũng hết, `active_window_count==0`, `enter_opening()` chạy → log `RLx: all train signals -> STOP (crossing reopening)` rồi `RLx: commanding gates UP (simulated motion, 3000 ms)` xuất hiện đúng ở t≈30s, không sớm hơn.

### TC-SC04B-3: OPENING → RECLOSING khi có tàu mới tiếp cận giữa lúc đang mở cổng
- **Loại**: Positive (2 điều kiện gần đồng thời: cổng đang mở dở dang + tàu mới xuất hiện)
- **Liên quan**: SC-04B, `OPENING --> RECLOSING : TRAIN_APPROACHING(direction) / stop opening, keep flashers active, register occupancy window, command gates down`
- **Môi trường**: (A)
- **Chuẩn bị**: Đưa RL1 tới `RLX_OPENING` (theo kịch bản TC-SC04B-2, ở t≈30s cổng bắt đầu mở, cần 3000ms để xác nhận mở — tức xác nhận dự kiến ở t≈33s).
- **Các bước**: Ở t≈31s (giữa lúc cổng đang mở dở dang, chưa xác nhận open), bấm `0`.
- **Kết quả mong đợi**: Log `RLx: reclosing - aborting gate-open motion, flashers remain active` xuất hiện ngay lập tức; ngay sau đó `RLx: commanding gates DOWN (simulated motion, 3000 ms)` (gọi lại `rlx_gate_command_close()`, huỷ motion mở dở dang). Không có `RLx: flashers OFF` nào xuất hiện xen giữa (flashers phải giữ nguyên trạng thái active suốt, đúng "keep flashers active"). Khoảng 3s sau (t≈34s), `reclose_confirmation` → `RLx: train signal PROCEED for direction 0 ...` → state về CLOSED (map ra CROSSING_CLOSED, giống hệt sau CLOSING thường).

### TC-SC04B-4: OPENING → FAULT khi cổng không xác nhận mở đúng hạn, rồi hồi phục qua FAULT → OPEN
- **Loại**: Negative (vào FAULT) + Positive (ra khỏi FAULT — đường hồi phục hoạt động, khác hẳn Lx)
- **Liên quan**: SC-04B, `OPENING --> FAULT : gates fail to confirm OPEN / hold last confirmed safe outputs and report fault`; `FAULT --> OPEN : verified repair and accepted local fault-clear request [crossing safe]`
- **Môi trường**: (A) là đủ (dùng phím demo `f`); lặp lại ở (B) để test đường IPC `MSG_REQUEST_FAULT_CLEAR` thật từ C1 nếu cần.
- **Chuẩn bị**: Đưa RL1 tới ngay trước lúc vào OPENING (ví dụ dừng ở cuối TC-SC04B-2, ngay trước t≈30s).
- **Các bước**:
  1. Trước khi cửa sổ occupancy cuối cùng hết hạn, bấm `x` (arm demo fault cho lần motion open sắp tới).
  2. Chờ cửa sổ hết hạn → `enter_opening()` chạy, `rlx_gate_command_open()` được gọi, nhưng do đã arm fault, không xác nhận open (`g_confirmed_open` giữ 0).
  3. Chờ đủ `RLX_OPENING_DEADLINE_MS`=15000ms kể từ lúc vào OPENING.
  4. Sau khi FAULT xuất hiện (`enter_fault()` gọi lại `rlx_gate_command_close()` — regression tương tự TC-SC04A-5), **thử** `f` (demo fault-clear) ngay lập tức mà **không** đợi gate xác nhận đóng xong.
  5. Đợi đủ 3000ms để gate đóng hoàn tất (vì `enter_fault()` vừa ra lệnh đóng), rồi thử `f` lần nữa — nhưng lưu ý `rlx_fsm_on_fault_clear()` chỉ chấp nhận khi `gates_confirmed_open()==1`, tức cần một lệnh `rlx_gate_command_open()` thành công (không bị `x` chặn) trước đó.
  6. Bấm `x` **không được** bấm lần này, sau đó cần một cách hợp lệ để đưa gate về trạng thái confirmed-open thật trước khi fault-clear được chấp nhận — thực tế thao tác đúng: gọi `f` sẽ tự kiểm tra `gates_confirmed_open()`; vì hiện tại gate đang ở trạng thái đóng (do fault ép đóng), fault-clear **phải bị từ chối** ở bước 4/5.
- **Kết quả mong đợi**:
  - Bước 3: log `RLx: FAULT latched (fault bit 0x1) ...` xuất hiện đúng ở t≈15s kể từ lúc vào OPENING.
  - Bước 4/5 (gate hiện đang CLOSED do fault, chưa từng OPEN thật): `rlx_fsm_on_fault_clear()` trả `RESULT_NACK`, `NACK_REASON_FAULT_ACTIVE` (log `[rlx_sensor] fault-clear result=... reason=...`) — **đúng theo thiết kế**, vì RC-10 yêu cầu xác minh gate thật sự an toàn (confirmed open) trước khi chấp nhận fault-clear, và crossing đang FAULT với gate đóng thì hiển nhiên chưa "verified repair".
  - Đây là điểm khác biệt quan trọng cần ghi chú so với TC-SC03A-6 Phần 2 của Lx: RLx **có** một đường hồi phục hoạt động thật (`MSG_REQUEST_FAULT_CLEAR`/phím `f`), chỉ là nó đòi hỏi điều kiện an toàn thật (gate confirmed open) — không phải là "không có cách nào" như trường hợp Lx. Để hoàn tất việc test nhánh ACK thành công của transition này, nhóm cần một kịch bản riêng mô phỏng "gate đã thực sự sửa xong và về vị trí mở" — vì `rlx_gate.c` không có API "force confirmed open" độc lập với `rlx_gate_command_open()`, cách khả thi duy nhất trong PoC hiện tại là: sau khi FAULT xuất hiện, **không** bấm `x` nữa, và chờ một chu trình OPENING mới được kích hoạt lại tự nhiên — nhưng vì state đang là `RLX_FAULT` (latched, bỏ qua mọi `TRAIN_APPROACHING` mới), **không có** cơ chế nào trong code hiện tại tự động thử lại `rlx_gate_command_open()` sau khi vào FAULT. => **Known gap bổ sung**: nhánh `FAULT --> OPEN` chỉ thực sự kiểm chứng được bằng test nếu FAULT được kích hoạt từ một nguyên nhân **không** liên quan tới gate-confirm-open (ví dụ watchdog trip trong lúc gate đang thực sự confirmed open sẵn) — xem biến thể dưới.
- **Biến thể để có nhánh ACK thật (bổ sung)**: Từ `RLX_OPEN` (gate đã confirmed open sẵn, `g_confirmed_open=1` từ `rlx_gate_init()`), kích hoạt fault bằng watchdog thay vì gate: tạm dừng tiến trình `rlx_main 1` bằng `kill -STOP`/`kill -CONT` như cách làm với Lx (RLx cũng có `rlx_fsm_report_watchdog_trip()` gọi từ một watchdog thread tương tự — kiểm tra `app/railway/src/rlx_watchdog.c` nếu tồn tại). Vì `enter_fault()` luôn gọi `rlx_gate_command_close()` bất kể lý do fault, gate sẽ chuyển từ open sang đóng (3s) rồi confirmed closed — tức **cùng vướng vấn đề trên**: `gates_confirmed_open()` sẽ là 0 ngay sau đó. Do đó, thực tế **fault-clear ACK chỉ khả thi** nếu operator đợi đủ để... **không có đường nào** trong code hiện tại tự chuyển gate về lại `g_confirmed_open=1` khi đang ở FAULT (không có lệnh `rlx_gate_command_open()` nào được gọi cho tới khi rời FAULT). => Ghi nhận rõ trong báo cáo: **nhánh `FAULT --> OPEN` hiện tại KHÔNG THỂ đạt `RESULT_ACK` qua bất kỳ chuỗi thao tác nào bằng giao diện hiện có**, vì điều kiện tiên quyết `gates_confirmed_open()==1` không bao giờ tự nhiên đúng một khi đã vào FAULT (mọi đường vào FAULT đều ép gate đóng, và không gì tự mở lại gate trong khi FAULT). Đây là **known gap quan trọng cần báo cáo cho giảng viên/nhóm**, tương tự nhưng độc lập với gap của Lx ở TC-SC03A-6.

### TC-SC04B-5 (Known gap, ghi nhận ngắn gọn): CLOSED/TRAIN_PRESENT → FAULT do "gate state contradicts CLOSED" không thể kích hoạt qua công cụ demo hiện có
- **Loại**: Negative / Known gap
- **Liên quan**: SC-04B, `CLOSED --> FAULT : gate state contradicts CLOSED`, `TRAIN_PRESENT --> FAULT : gate state contradicts CLOSED`
- **Môi trường**: (A)
- **Ghi chú thay cho các bước**: `check_gate_contradiction_closed()` chỉ trả fault khi `gates_confirmed_closed()==0` trong lúc state là CLOSED/TRAIN_PRESENT. Nhưng `rlx_gate.c` chỉ đổi `g_confirmed_closed` qua đúng hai lệnh `rlx_gate_command_close()`/`rlx_gate_command_open()`, cả hai đều chỉ được gọi từ chính `rlx_fsm.c` theo logic đã biết (không có lệnh mở cổng nào chạy trong lúc đang CLOSED/TRAIN_PRESENT). Vì vậy, **không tồn tại chuỗi phím/IPC nào trong bản hiện tại** khiến gate "tự nhiên" mâu thuẫn với CLOSED trong khi state vẫn đang CLOSED — giống hệt lỗ hổng ở SC-04A's "WARNING→FAULT via timeout". Khuyến nghị: nếu muốn test nhánh này thật sự, cần thêm một API demo kiểu `rlx_gate_force_open_for_test()` — hiện chưa tồn tại, nên **không viết test case dương giả cho nhánh này**.

---

## 9. SC-05 — Central Connectivity and Local Autonomy

### TC-SC05-1: PA-07 — đúng 3 lần missed heartbeat liên tiếp mới đánh dấu UNAVAILABLE
- **Loại**: Positive + Edge case (biên đúng lần thứ 3, không phải lần thứ 2)
- **Liên quan**: SC-05, `CENTRAL_CONNECTED --> DEGRADED_LOCAL : three consecutive 1 s heartbeats missed`
- **Môi trường**: (B)
- **Chuẩn bị**: `c_main` và `lx_main 1` đang chạy bình thường, L1 đã xuất hiện `AVAILABLE` trên bảng C1.
- **Các bước**:
  1. Ghi lại pid của `lx_main 1`, gửi `kill -STOP <pid>` (dừng hẳn tiến trình — heartbeat 1Hz của nó sẽ ngừng hoàn toàn).
  2. Quan sát bảng C1 mỗi giây (tự refresh 1Hz): ngay sau 1 giây (`missed_heartbeat_ticks=1`) và 2 giây (`=2`), cột AVAILABILITY của L1 **vẫn phải là** `AVAILABLE`.
  3. Ngay tại giây thứ 3 (`missed_heartbeat_ticks==3`), quan sát log central: `Controller 1 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)` (ghi cả ra `central_log.txt`), và cột AVAILABILITY chuyển `UNAVAILABLE`.
  4. `kill -CONT <pid>` để dọn dẹp tiến trình cho các test sau.
- **Kết quả mong đợi**: Đúng như trên — biên chuyển trạng thái là lần miss thứ 3, không sớm hơn (test edge case: nếu UNAVAILABLE xuất hiện ở giây thứ 2, đó là bug vi phạm PA-07 "three consecutive").

### TC-SC05-2: DEGRADED_LOCAL — Lx/RLx tiếp tục hoạt động hoàn toàn bình thường khi Central không nhận được heartbeat
- **Loại**: Positive / Regression note
- **Liên quan**: SC-05, ghi chú "Connectivity loss alone never forces all-red, FLASHING_RED, or frozen timing — traffic, pedestrian, railway, and fault logic (SC-01 through SC-04) all continue locally"
- **Môi trường**: (B)
- **Chuẩn bị**: Tương tự TC-SC05-1 — `lx_main 1` bị `kill -STOP` (từ góc nhìn Central là "mất kết nối"/UNAVAILABLE), nhưng **lần này không STOP thật** vì cần L1 vẫn chạy để quan sát: thay vào đó, đơn giản là **không chạy `c_main` chút nào** trong suốt bài test (môi trường (A) độc lập cũng chứng minh được luận điểm này, vì `lx_fsm.c` không hề đọc `fsm->link_state` ở bất kỳ transition nào trong SC-01/02/03/04).
- **Các bước**: Chạy `lx_main 1` một mình (không `c_main`), bấm các phím sensor bình thường (`a`, `1`, `w`, v.v.) và quan sát toàn bộ chu kỳ ARTERIAL/CONNECTOR, WALK/FDW, vẫn hoạt động đúng thời lượng như các test ở mục 1-4 của tài liệu này.
- **Kết quả mong đợi**: Không có bất kỳ khác biệt hành vi nào so với khi có `c_main` chạy — xác nhận trực tiếp qua source: không có lệnh `if (fsm->link_state == ...)` nào xuất hiện trong toàn bộ `lx_fsm.c` chi phối phase/pedestrian/override/railway-preemption logic. Đây là behavior **đạt yêu cầu SC-05 note "by construction"**, không phải vì có cơ chế fallback chủ động nào được lập trình riêng.

### TC-SC05-3: Resync tức thời khi heartbeat khôi phục — và lỗ hổng hiển thị RESYNCHRONISING
- **Loại**: Positive + Known gap
- **Liên quan**: SC-05, `DEGRADED_LOCAL --> RESYNCHRONISING --> CENTRAL_CONNECTED`
- **Môi trường**: (B)
- **Chuẩn bị**: Lặp lại TC-SC05-1 cho tới khi L1 = `UNAVAILABLE`.
- **Các bước**:
  1. `kill -CONT <pid lx_main 1>` để L1 tiếp tục chạy và tự động gửi lại `MSG_HEARTBEAT` (1Hz, không cần thao tác gì thêm từ phía L1).
  2. Quan sát bảng C1 ở lần refresh 1Hz ngay sau khi heartbeat đầu tiên tới.
- **Kết quả mong đợi**: `c_server_record_status()` (được gọi từ `MSG_HEARTBEAT` case trong `c_main.c`) reset `missed_heartbeat_ticks=0` và `marked_unavailable=0` **ngay trên heartbeat đầu tiên nhận được** — AVAILABILITY chuyển thẳng `UNAVAILABLE → AVAILABLE` trong đúng 1 tick, không có bước trung gian nào hiển thị "đang resync". *Known gap*: SC-05 vẽ một trạng thái `RESYNCHRONISING` tường minh ("send complete current state to C1" / "C1 accepts complete state exchange"), nhưng code hiện tại không có STATUS "trạng thái đầy đủ" riêng biệt nào được gửi khi vừa kết nối lại — mọi `MSG_HEARTBEAT`/`MSG_STATUS` đều có cùng nội dung `status_report_payload_t`, và trường `link_state` trong đó **luôn bị hard-code** `LINK_CENTRAL_CONNECTED` bởi cả `lx_comm.c` (`req.payload.heartbeat.summary.link_state = (uint32_t)LINK_CENTRAL_CONNECTED;`) lẫn `rlx_comm.c`, bất kể tình trạng kết nối thật — nghĩa là cột hiển thị (nếu HMI từng in `link_state`) sẽ không bao giờ phản ánh đúng DEGRADED_LOCAL/RESYNCHRONISING dù Central có đang coi controller là UNAVAILABLE. Ghi rõ đây là giới hạn PoC đã biết, không phải bug mới phát hiện, cần nêu trong báo cáo nghiệm thu.

---

## Phụ lục — Tổng hợp Known Gaps phát hiện trong quá trình lập test plan

| Gap | Chart | Vị trí trong code | Ảnh hưởng |
|---|---|---|---|
| Không có đường phục hồi `FAULT_SAFE -> NORMAL_OPERATION` cho Lx qua bàn phím/IPC | SC-03A | `lx_fsm_local_fault_clear()` (lx_fsm.c) định nghĩa nhưng không được gọi ở đâu trong `lx_main.c`/`lx_sensor.c` | Một khi Lx vào FAULT_SAFE (watchdog trip), nó kẹt vĩnh viễn trong bản build hiện tại — xem TC-SC03A-6 |
| Nhánh `FAULT --> OPEN` của RLx không đạt được `RESULT_ACK` bằng bất kỳ chuỗi thao tác nào | SC-04B | `rlx_fsm_on_fault_clear()` yêu cầu `gates_confirmed_open()==1`, nhưng mọi đường vào FAULT (`enter_fault()`) đều ép gate đóng và không gì tự mở lại gate khi đang FAULT | Xem TC-SC04B-4 biến thể |
| `WARNING --> FAULT` (diagnostic timeout 60s) không thể kích hoạt | SC-04A | `RLX_WARNING_TO_CLOSING_MS` (5s) luôn bắn trước, reset `state_elapsed_ms`; sự kiện mô phỏng rời rạc không thể hiện sensor "kẹt active liên tục" | Dead code theo chính comment trong `rlx_fsm.c` |
| `CLOSED/TRAIN_PRESENT --> FAULT` do gate mâu thuẫn không thể kích hoạt qua demo hiện có | SC-04B | `rlx_gate.c` chỉ đổi trạng thái confirm qua lệnh do chính `rlx_fsm.c` phát ra | Cần thêm API demo nếu muốn test thật |
| `REQUEST_LATCHED` "stuck-active beyond diagnostic timeout" (PA-03) không được phát hiện | SC-02 | `lx_fsm_latch_pedestrian_request()` có comment "KNOWN LIMITATION" xác nhận `FAULT_PED_BUTTON_STUCK` không bao giờ được set | Không viết test dương cho nhánh này trong tài liệu này |
| `link_state` luôn hard-code `LINK_CENTRAL_CONNECTED` trong mọi heartbeat gửi đi | SC-05 | `lx_comm.c`, `rlx_comm.c` | RESYNCHRONISING không thể quan sát trực tiếp qua trường này; chỉ suy luận gián tiếp qua AVAILABILITY trên C1 |

Tổng số test case trong tài liệu này: **42** (đếm cả các biến thể/edge case lồng trong một số TC).
