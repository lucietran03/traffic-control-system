# Test Plan 05 - An toàn / Xử lý lỗi (Fault-Safety)

Phạm vi tài liệu này: các cơ chế an toàn và xử lý lỗi của hệ thống điều
khiển giao thông QNX - watchdog nội bộ (PA-10), giám sát FAULT_SAFE tại
giao lộ (Lx), luồng lỗi "gate không xác nhận đóng/mở" tại đường ngang xe
lửa (RC-06), báo cáo lỗi + xóa lỗi (RC-09/RC-10), và giới hạn đã biết về
cảm biến "kẹt" (stuck-sensor).

Toàn bộ test case dưới đây được viết dựa trên việc đọc trực tiếp mã nguồn
hiện tại (không suy đoán):
- `app/intersection/src/lx_watchdog.c`, `app/intersection/src/lx_fsm.c`
- `app/railway/src/rlx_watchdog.c`, `app/railway/src/rlx_gate.c`,
  `app/railway/src/rlx_fsm.c`, `app/railway/src/rlx_sensor.c`
- `app/central/src/c_operator.c`, `app/central/src/c_comm.c`,
  `app/central/src/c_server.c`, `app/central/src/c_main.c`

Nguyên tắc xuyên suốt tài liệu: **trung thực hơn số lượng**. Nếu một kịch
bản không có cách kích hoạt thật qua bàn phím/IPC trong bản hiện tại, test
case sẽ ghi rõ điều đó và đề xuất cách gần nhất có thể (review code, dùng
debugger, v.v.) thay vì bịa ra một phím/luồng không tồn tại.

## Quy ước môi trường (A/B/C)

Theo `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`, mục "Deployment Topologies":

- **(A) 1 node đơn**: chỉ chạy MỘT tiến trình (`rlx_main` hoặc `lx_main`)
  độc lập trên một máy/VM QNX, không cần `c_main`. Quan sát hành vi qua
  chính console/stderr của tiến trình đó (ví dụ các dòng in ra bởi
  `rlx_gate.c`, `rlx_fsm.c`, `rlx_watchdog.c`) và qua các phím mô phỏng
  cảm biến của `rlx_sensor.c`/`lx_sensor.c`. Dùng cho các test không cần
  Central ra lệnh.
- **(B) Nhiều node cùng một máy QNX**: chạy `c_main` + `rlx_main`
  (+ `lx_main` nếu cần) trên **cùng một** VM/máy QNX (cùng node Qnet,
  không cần khai báo `TRAFFIC_NODE_MAP` vì `name_open()` coi nhau là
  "same node"). Dùng cho các test cần Central gửi lệnh IPC thật
  (`MSG_REQUEST_FAULT_CLEAR`, `MSG_SET_MODE`, ...) tới Lx/RLx.
- **(C) Nhiều máy/VM QNX qua mạng thật**: giống (B) nhưng `c_main` và
  `lx_main`/`rlx_main` chạy trên các VM/máy vật lý khác nhau, kết nối qua
  Qnet thật (Case 1/2/3 trong QNX_DEPLOYMENT_RUN_GUIDE.md), cần export
  `TRAFFIC_NODE_MAP` đúng theo hướng dẫn. Dùng để xác nhận lại rằng hành
  vi an toàn không đổi khi có độ trễ mạng/Qnet thật xen vào, sau khi đã
  PASS ở môi trường (B).

Khuyến nghị: chạy toàn bộ test ở (A)/(B) trước; chỉ cần lặp lại một tập
con đại diện (RC-06 dương tính, REQUEST_FAULT_CLEAR NACK/ACK, watchdog)
ở (C) để xác nhận không có khác biệt do mạng thật.

## Hằng số thời gian liên quan (tra từ code, không suy đoán)

| Hằng số | Giá trị | Nguồn |
|---|---|---|
| `RLX_WARNING_TO_CLOSING_MS` | 5 s | `rlx_timer.h` |
| `RLX_GATE_MOTION_MS` | 3 s | `rlx_gate.h` |
| `RLX_CLOSING_DEADLINE_MS` | 15 s | `rlx_timer.h` |
| `RLX_OPENING_DEADLINE_MS` | 15 s | `rlx_timer.h` |
| `RLX_EXPECTED_ARRIVAL_MS` | 20 s | `rlx_timer.h` |
| `RLX_OCCUPANCY_WINDOW_MS` | 20 s | `rlx_timer.h` |
| Chu kỳ tick RLx (`IPC_PULSE_RAILWAY_WARNING`) | 1 s | `rlx_main.c` |
| Chu kỳ kiểm tra watchdog RLx | 3 s (`RLX_WATCHDOG_CHECK_INTERVAL_S`) | `rlx_watchdog.c` |
| Chu kỳ tick Lx (`IPC_PULSE_PHASE_TIMER`) | 100 ms | `lx_main.c` |
| Chu kỳ kiểm tra watchdog Lx | 2 s (`LX_WATCHDOG_CHECK_INTERVAL_S`) | `lx_watchdog.c` |

Lưu ý về độ trễ phát hiện watchdog: vòng lặp watchdog không đồng bộ với
thời điểm bắt đầu "treo" (nó chỉ `sleep()` rồi so sánh bộ đếm với lần
trước). Vì vậy độ trễ phát hiện thực tế nằm trong khoảng 1-2 chu kỳ kiểm
tra, tức khoảng 2-4 s cho Lx và 3-6 s cho RLx, không phải chính xác 2 s/3 s.

---

## Nhóm 1 - RC-06: Gate không xác nhận đóng/mở

Cơ chế demo có sẵn: `rlx_gate_arm_demo_fault()` (`rlx_gate.c`) đặt cờ
một-lần `g_demo_fault_armed`; **lần di chuyển gate kế tiếp** (đóng hoặc
mở, tùy cái nào được lệnh trước) sẽ không bao giờ xác nhận xong
(`g_confirmed_closed`/`g_confirmed_open` giữ nguyên 0), buộc
`rlx_fsm.c` phải tự phát hiện qua deadline (`RLX_CLOSING_DEADLINE_MS`/
`RLX_OPENING_DEADLINE_MS`) và gọi `enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
Phím kích hoạt: `x` trong `rlx_sensor.c`.

### TC-FAULT-01: RC-06 - gate không xác nhận đóng khi tàu tới -> FAULT, không bao giờ PROCEED
- **Loại**: Positive
- **Liên quan**: RC-06, RC-03
- **Môi trường**: (A) hoặc (B)
- **Chuẩn bị**: Khởi động `rlx_main 1` (RL1), crossing ở trạng thái nghỉ
  `RLX_OPEN` (mặc định lúc khởi động).
- **Các bước**:
  1. Bấm `x` (arm demo fault cho lần di chuyển gate kế tiếp).
  2. Bấm `0` (TRAIN_APPROACHING hướng 0) -> RLx vào `RLX_WARNING`, đèn
     flasher bật (`rlx_signal_show_flashers_on`).
  3. Chờ 5 s (`RLX_WARNING_TO_CLOSING_MS`) -> RLx tự chuyển sang
     `RLX_CLOSING`, gọi `rlx_gate_command_close()` (log
     "commanding gates DOWN..."). Vì đã arm ở bước 1, motion này được
     đánh dấu `g_fail_this_motion = 1`.
  4. Sau 3 s di chuyển mô phỏng (`RLX_GATE_MOTION_MS`), quan sát log
     "gate FAILED TO CONFIRM (simulated fault) ..." thay vì gate xác
     nhận đóng.
  5. Tiếp tục chờ đến khi tổng thời gian ở `RLX_CLOSING` đạt
     `RLX_CLOSING_DEADLINE_MS` = 15 s kể từ lúc vào CLOSING (tức khoảng
     10 s sau bước 4).
- **Kết quả mong đợi**:
  - Ngay khi `state_elapsed_ms >= 15000` mà `gates_confirmed_closed()`
    vẫn là 0, `check_closing_or_reclosing_complete()` gọi
    `enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - Log in ra lệnh đóng gate lại (`enter_fault()` gọi
    `rlx_gate_command_close()` một cách vô điều kiện) và
    `rlx_signal_show_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - `fsm->state == RLX_FAULT`, `fsm->faults` có bit
    `FAULT_GATE_CONFIRM_MISSING` set, `fault_report_pending = 1`.
  - **Không bao giờ** có log `rlx_signal_show_train_proceed(...)` (hàm
    PROCEED chỉ được gọi trong `check_closing_or_reclosing_complete()`
    khi gate xác nhận đóng thành công - nhánh này không được chạy tới).
  - Nếu chạy ở môi trường (B), RL1 gửi `MSG_FAULT_REPORT` ngay trong
    cùng tick đó (`rlx_main.c` gọi `rlx_comm_send_fault_report()` ngay
    sau `rlx_fsm_on_tick()`); Central log dòng
    `FAULT_REPORT from <RL1>: fault_code=... severity=... detail="..."`.

### TC-FAULT-02: Baseline (control) - đóng gate bình thường không lỗi -> PROCEED đúng
- **Loại**: Negative (thực chất là bài kiểm tra đối chứng / sanity)
- **Liên quan**: RC-06 (đối chứng để chắc chắn phép đo ở TC-FAULT-01 có ý nghĩa)
- **Môi trường**: (A) hoặc (B)
- **Chuẩn bị**: `rlx_main 1`, **không** bấm `x`.
- **Các bước**: Bấm `0` -> chờ 5 s -> CLOSING -> chờ 3 s cho gate xác
  nhận đóng bình thường.
- **Kết quả mong đợi**: Sau đúng ~3 s (không phải 15 s), gate xác nhận
  đóng, `fsm->state -> RLX_CLOSED`, log
  `rlx_signal_show_train_proceed(direction=0)` được gọi, KHÔNG có fault
  nào được set. Test này chứng minh rằng deadline 15 s ở TC-FAULT-01 chỉ
  bị vi phạm vì cờ demo-fault, không phải vì lỗi ngẫu nhiên trong logic
  timing bình thường.

### TC-FAULT-03: RC-06 edge case - gate không xác nhận đóng lại khi tàu thứ 2 tới trong lúc đang OPENING (RECLOSING)
- **Loại**: Edge case
- **Liên quan**: RC-06, RC-04
- **Môi trường**: (A) hoặc (B)
- **Chuẩn bị**: Đưa RL1 qua một chu kỳ WARNING->CLOSING->CLOSED->
  TRAIN_PRESENT->OPENING bình thường (bấm `0`, đợi đủ để tàu "đi qua"
  hết cửa sổ chiếm dụng 20 s, RLx tự vào `RLX_OPENING` và gọi
  `rlx_gate_command_open()`).
- **Các bước**:
  1. Ngay khi RLx vừa vào `RLX_OPENING` (gate đang mở dở, chưa xác nhận
     mở xong), bấm `x` để arm demo fault cho lần di chuyển gate KẾ TIẾP.
  2. Ngay sau đó bấm `1` (TRAIN_APPROACHING hướng 1) trong lúc vẫn còn
     ở `RLX_OPENING`.
  3. Theo `rlx_fsm_simulate_train_approaching()`, case `RLX_OPENING` gọi
     `enter_reclosing()`, tức lập tức `rlx_gate_command_close()` một
     motion MỚI - và vì đã arm ở bước 1, motion đóng này sẽ không xác
     nhận xong.
  4. Chờ đủ `RLX_CLOSING_DEADLINE_MS` = 15 s kể từ lúc vào `RLX_RECLOSING`.
- **Kết quả mong đợi**: `check_closing_or_reclosing_complete()` (dùng
  chung cho cả CLOSING và RECLOSING) phát hiện deadline vượt quá ->
  `enter_fault(FAULT_GATE_CONFIRM_MISSING)`, giống hệt nhánh CLOSING.
  Trong lúc chờ, trạng thái báo cáo ra ngoài (`map_to_crossing_state`)
  phải là `CROSSING_WARNING` cho cả `RLX_RECLOSING` (đúng theo comment
  compliance-fix trong `map_to_crossing_state()`), không được nhảy thẳng
  lên `CROSSING_CLOSED` trước khi thực sự xác nhận đóng.

---

## Nhóm 2 - Fault buộc gate đóng lại ngay (bugfix vừa sửa trong `enter_fault()`)

`enter_fault()` hiện gọi `rlx_gate_command_close()` một cách **vô điều
kiện** trước khi set `fsm->state = RLX_FAULT`, để không bao giờ để gate ở
nguyên vị trí đang mở/mở dở khi có lỗi (trước khi sửa, lỗi chỉ set cờ và
in "gates held as-is", để hở crossing khi lỗi xảy ra lúc đang OPEN/OPENING).

### TC-FAULT-04: Lỗi phát sinh khi gate đang OPENING -> phải được lệnh đóng lại ngay
- **Loại**: Positive (regression test cho bugfix)
- **Liên quan**: RC-06, RC-10, PA-10 (fail-safe output)
- **Môi trường**: (A) hoặc (B)
- **Chuẩn bị**: Đưa RL1 vào `RLX_TRAIN_PRESENT` với đúng 1 cửa sổ chiếm
  dụng đang chạy (bấm `0`, để chu kỳ WARNING(5s)->CLOSING(3s, KHÔNG arm
  fault ở bước này để CLOSING thành công bình thường)->CLOSED->
  TRAIN_PRESENT diễn ra tự nhiên).
- **Các bước**:
  1. Trong lúc đang ở `RLX_TRAIN_PRESENT` (gate đã xác nhận đóng, chưa
     có lệnh mở nào được gửi), bấm `x` để arm demo fault cho lần di
     chuyển KẾ TIẾP (chính là lần mở gate sắp tới).
  2. Chờ đến khi cửa sổ chiếm dụng 20 s (`RLX_OCCUPANCY_WINDOW_MS`) hết
     hạn -> `active_window_count == 0` -> RLx tự gọi `enter_opening()`,
     tức `rlx_gate_command_open()` (log "commanding gates UP...").
  3. Vì đã arm ở bước 1, motion mở này không bao giờ xác nhận xong. Chờ
     đủ `RLX_OPENING_DEADLINE_MS` = 15 s.
- **Kết quả mong đợi**:
  - `check_opening_complete()` phát hiện deadline vượt quá trong khi
    `fsm->state == RLX_OPENING` -> gọi
    `enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - Quan sát log: một dòng "commanding gates DOWN (simulated motion,
    3000 ms)" MỚI xuất hiện ngay tại thời điểm vào FAULT - đây chính là
    hành vi của bugfix (trước khi sửa, sẽ không có lệnh đóng lại nào,
    gate coi như bị bỏ mặc ở trạng thái đang mở/nửa mở).
  - Vì `rlx_gate_command_close()` reset `g_confirmed_open = 0` và bắt
    đầu motion đóng mới (không bị arm fault lần này, trừ khi tester chủ
    động bấm `x` lại), sau thêm 3 s gate sẽ xác nhận đóng thành công
    (`g_confirmed_closed = 1`) dù FSM đã đứng yên ở `RLX_FAULT` (không
    còn xử lý tick logic ngoài `rlx_gate_on_tick()` vốn chạy vô điều
    kiện mỗi tick).
  - Tổng thời gian chờ dự kiến cho toàn bộ kịch bản: khoảng
    5 + 3 + 20 + 15 ≈ 43 s kể từ lúc bấm `0` đến khi vào FAULT.

---

## Nhóm 3 - Central REQUEST_FAULT_CLEAR (RC-09/RC-10: không bao giờ bỏ qua xác minh thực)

`rlx_fsm_on_fault_clear()` chỉ ACK khi `gates_confirmed_open()` trả về
true tại **thời điểm xử lý yêu cầu**, đọc trực tiếp từ `rlx_gate.c`
(không dùng giá trị cache nào khác) - đây chính là điều RC-10 yêu cầu
("never bypass live verification").

**Phát hiện quan trọng khi đọc code**: một khi FSM đã vào `RLX_FAULT`,
`enter_fault()` luôn luôn ra lệnh ĐÓNG gate (không bao giờ mở), và nhánh
`RLX_FAULT` trong `rlx_fsm_on_tick()` không làm gì (không có lệnh mở gate
nào được phát trong khi đang fault). `rlx_gate_command_open()` trong toàn
bộ codebase hiện tại **chỉ** được gọi từ `enter_opening()`
(`rlx_fsm.c`), và hàm đó **chỉ** được gọi từ nhánh `RLX_TRAIN_PRESENT`
của `rlx_fsm_on_tick()` - không bao giờ chạy khi `fsm->state == RLX_FAULT`.
Nói cách khác: **không có phím bấm hay luồng logic nào trong ứng dụng
hiện tại có thể tự đưa gate về trạng thái "confirmed open" trong khi RLx
đang ở RLX_FAULT.** Đây không phải lỗi RC-10 (ngược lại, đúng ra là hệ
quả tất yếu của RC-10: "gate luôn phải được xác nhận lại bằng tay/thực
tế trước khi coi là an toàn để mở lại") nhưng nó khiến nhánh ACK của
`rlx_fsm_on_fault_clear()` **không thể tái hiện qua bàn phím** trong bản
build hiện tại. TC-FAULT-07 dưới đây ghi nhận trung thực giới hạn này và
đề xuất cách khả dĩ nhất (dùng debugger) để vẫn kiểm thử được nhánh đó
trên máy QNX thật.

### TC-FAULT-05: REQUEST_FAULT_CLEAR khi gate CHƯA xác nhận mở -> NACK (RC-10 core)
- **Loại**: Negative
- **Liên quan**: RC-09, RC-10
- **Môi trường**: (B) - cần Central thật để gửi `MSG_REQUEST_FAULT_CLEAR`
- **Chuẩn bị**: Đưa RL1 vào `RLX_FAULT` bằng TC-FAULT-01 (hoặc bất kỳ
  kịch bản nào ở Nhóm 1/2). Gate lúc này đã được `enter_fault()` lệnh
  đóng - sau 3 s nó sẽ tự xác nhận ĐÓNG (không phải MỞ).
- **Các bước**: Tại console Central (`c_main`), bấm `f` ->
  `RLx number (1-3): 1` (nhập `1` cho RL1) -> Enter.
- **Kết quả mong đợi**:
  - RL1 nhận `MSG_REQUEST_FAULT_CLEAR`, gọi `rlx_fsm_on_fault_clear()`.
  - Vì `fsm->state == RLX_FAULT` nhưng `gates_confirmed_open() == 0`
    (gate đang đóng, không phải mở), trả về
    `RESULT_NACK` / `NACK_REASON_FAULT_ACTIVE`.
  - Central log: `C1: REQUEST_FAULT_CLEAR to <RL1> -> NACK reason=FAULT_ACTIVE`
    (theo `on_command_reply()` trong `c_comm.c`).
  - `fsm->state` vẫn là `RLX_FAULT`, `fsm->faults` không đổi.

### TC-FAULT-06: REQUEST_FAULT_CLEAR khi RLx KHÔNG hề đang fault -> NACK UNKNOWN_TARGET
- **Loại**: Negative (edge case)
- **Liên quan**: RC-09
- **Môi trường**: (B)
- **Chuẩn bị**: RL1 ở trạng thái bình thường (`RLX_OPEN`, không fault).
- **Các bước**: Từ Central, bấm `f` -> nhập RLx = 1.
- **Kết quả mong đợi**: `rlx_fsm_on_fault_clear()` thấy
  `fsm->state != RLX_FAULT` -> `RESULT_NACK` /
  `NACK_REASON_UNKNOWN_TARGET`. Central log
  `... -> NACK reason=UNKNOWN_TARGET`. Không có thay đổi trạng thái nào.

### TC-FAULT-07: REQUEST_FAULT_CLEAR khi gate ĐÃ thực sự xác nhận mở -> ACK, thoát FAULT
- **Loại**: Positive - **cần công cụ debug, không có phím bấm tương ứng trong bản hiện tại (xem phần phân tích ở đầu Nhóm 3)**
- **Liên quan**: RC-09, RC-10
- **Môi trường**: (B), cộng thêm debugger (gdb/QNX Momentics debugger,
  hoặc `pdebug` + `qnx-gdb` từ host) đính kèm vào tiến trình `rlx_main`
  đang chạy trên target QNX. Build `rlx_main` với thông tin debug (không
  strip) để `call` hàm theo tên còn dùng được.
- **Chuẩn bị**: Đưa RL1 vào `RLX_FAULT` (TC-FAULT-01/04).
- **Các bước**:
  1. Đính kèm debugger vào tiến trình `rlx_main` (không cần dừng nó lâu).
  2. Gọi trực tiếp hàm public `rlx_gate_command_open()` qua debugger, ví
     dụ trong gdb: `call rlx_gate_command_open()`. Hàm này set
     `g_motion = GATE_MOVING_OPEN`, `g_remaining_ms = 3000`,
     `g_fail_this_motion = g_demo_fault_armed` (đảm bảo KHÔNG bấm `x`
     trước đó, để `g_demo_fault_armed == 0` và motion này thành công).
  3. `continue`/thả tiến trình chạy tiếp. `rlx_gate_on_tick()` được gọi
     vô điều kiện mỗi tick (kể cả khi `fsm->state == RLX_FAULT`, vì lệnh
     gọi này nằm TRƯỚC switch trên `fsm->state` trong
     `rlx_fsm_on_tick()`), nên motion mở sẽ tự tiến triển và sau 3 s
     (`RLX_GATE_MOTION_MS`) `g_confirmed_open` chuyển thành 1, dù FSM
     vẫn đứng yên ở `RLX_FAULT`.
  4. Từ Central, bấm `f` -> nhập RLx = 1.
- **Kết quả mong đợi**:
  - `rlx_fsm_on_fault_clear()` thấy `fsm->state == RLX_FAULT` VÀ
    `gates_confirmed_open() == 1` (đọc live, không cache) -> trả về
    `RESULT_ACK`.
  - `fsm->state -> RLX_OPEN`, `fsm->state_elapsed_ms = 0`,
    `fsm->faults = FAULT_NONE`, `active_window_count = 0`, cả 2 slot
    `windows[]` bị reset (`active = 0`) - đúng theo code xóa "stale
    occupancy bookkeeping" khi clear fault.
  - Central log: `C1: REQUEST_FAULT_CLEAR to <RL1> -> ACK`.
  - **Nếu không có sẵn debugger/không thể attach vào target QNX**: bỏ
    qua phần thực thi runtime của test case này, chỉ verify bằng code
    review rằng nhánh ACK trong `rlx_fsm_on_fault_clear()` đọc
    `gates_confirmed_open()` (không phải một cờ cache) tại đúng thời
    điểm xử lý request - ghi rõ trong biên bản test là "verified by code
    review only, nhánh ACK không thể tái hiện qua UI hiện tại".

### TC-FAULT-08: Gửi REQUEST_FAULT_CLEAR lặp lại liên tiếp khi vẫn chưa đủ điều kiện -> luôn NACK, không có tác dụng phụ
- **Loại**: Negative / idempotency edge case
- **Liên quan**: RC-09, RC-10
- **Môi trường**: (B)
- **Chuẩn bị**: Giống TC-FAULT-05 (RL1 đang `RLX_FAULT`, gate đóng).
- **Các bước**: Từ Central, bấm `f` -> RLx=1 ba lần liên tiếp (không đợi
  lâu giữa các lần).
- **Kết quả mong đợi**: Cả 3 lần đều nhận `NACK reason=FAULT_ACTIVE`.
  `fsm->faults`, `fsm->state`, `active_window_count` không bị thay đổi/hư
  hỏng bởi các lần gọi lặp (không có state được ghi đè một phần rồi bỏ
  dở - nhánh NACK không chạm vào field nào khác ngoài `reply`).

### TC-FAULT-09: Phím `f` cục bộ tại `rlx_sensor.c` vẫn tuân thủ RC-10 (không phải "cửa sau" bỏ qua xác minh)
- **Loại**: Negative (làm rõ hiểu lầm tiềm ẩn)
- **Liên quan**: RC-09, RC-10
- **Môi trường**: (A) hoặc (B) - không bắt buộc cần Central, vì đây là
  trigger cục bộ tại chính tiến trình RLx.
- **Chuẩn bị**: RL1 đang `RLX_FAULT`, gate đóng (chưa xác nhận mở) - như
  TC-FAULT-05.
- **Các bước**: Tại console của chính `rlx_main 1`, bấm phím `f`.
  Theo comment trong `rlx_sensor.c`: đây là "DEMO-ONLY local fault-clear
  trigger (bypasses the real MSG_REQUEST_FAULT_CLEAR path)" - tức chỉ
  bỏ qua **đường truyền IPC từ Central**, mô phỏng việc kỹ thuật viên có
  mặt trực tiếp tại tủ điều khiển đường ngang, KHÔNG phải bỏ qua bước
  xác minh an toàn.
- **Kết quả mong đợi**: Phím `f` cục bộ gọi thẳng
  `rlx_fsm_on_fault_clear(fsm, &reply)` - **cùng một hàm, cùng logic xác
  minh live** như khi Central gọi qua IPC. Vì gate chưa xác nhận mở, kết
  quả vẫn là `RESULT_NACK`/`NACK_REASON_FAULT_ACTIVE`, in ra
  `[rlx_sensor] fault-clear result=... reason=...`. Test này chứng minh
  RC-10 được áp dụng đồng nhất bất kể nguồn gốc yêu cầu là Central hay
  thao tác cục bộ - "local" ở đây chỉ khác về đường đi của yêu cầu, không
  phải một lối tắt an toàn khác.

### TC-FAULT-10: Phím `f` cục bộ ACK khi gate thực sự đã mở (đối chứng dương cho TC-FAULT-09)
- **Loại**: Positive - cần debugger như TC-FAULT-07
- **Liên quan**: RC-09, RC-10
- **Môi trường**: (A), cộng debugger như TC-FAULT-07
- **Các bước**: Lặp lại bước 1-3 của TC-FAULT-07 (dùng debugger ép gate
  xác nhận mở trong khi vẫn `RLX_FAULT`), sau đó bấm `f` ngay tại
  console của `rlx_main` thay vì qua Central.
- **Kết quả mong đợi**: `RESULT_ACK`, `fsm->state -> RLX_OPEN`, giống hệt
  kết quả của TC-FAULT-07 nhưng đến từ đường cục bộ - xác nhận hai đường
  (Central IPC và phím cục bộ) chia sẻ đúng một điểm thực thi logic an
  toàn (`rlx_fsm_on_fault_clear()`), không có bản sao logic bị lệch nhau.

---

## Nhóm 4 - FAULT_SAFE tại giao lộ (Lx)

**Phát hiện khi đọc code**: `app/intersection/src/lx_sensor.c` chỉ có
các phím `a/A/c/C/1/2/3/4/w/W/h/?/q` - hoàn toàn không có phím nào gọi
tới bất kỳ hàm nào raise fault tại Lx. Con đường DUY NHẤT hiện có để Lx
vào `SUPERVISORY_FAULT_SAFE` là qua `lx_fsm_report_watchdog_trip()`,
được gọi từ `lx_watchdog_thread()` khi watchdog thật sự trip (xem Nhóm 5).

**Cập nhật (test-plan finding đã được sửa)**: mục này từng ghi nhận rằng
`lx_fsm_local_fault_clear()` tồn tại nhưng không được gọi ở bất kỳ đâu -
tức Lx không có đường thoát khỏi `FAULT_SAFE` nào khác ngoài khởi động
lại tiến trình. Điều đó không còn đúng: `MSG_REQUEST_FAULT_CLEAR` nay
được `lx_main.c`'s `on_request()` xử lý, gọi `lx_fsm_on_request_fault_
clear()` (`lx_fsm.c`/`lx_fsm.h`), và `c_operator.c`'s phím `f` hỏi
`node type` (0=Lx, 1=RLx) trước khi hỏi số hiệu, nên có thể nhắm một Lx
trực tiếp từ console C1. Khác với RLx's `rlx_fsm_on_fault_clear()` (yêu
cầu `gates_confirmed_open()==1`), hàm phía Lx không có điều kiện vật lý
nào phải re-verify - nó unconditional/idempotent: luôn ACK, xóa
`fsm->faults`, và chỉ đổi `supervisory` nếu đang thực sự `FAULT_SAFE`.
Một fix an toàn liên quan (re-audit finding, xem `last_crossing_state`
trong `lx_fsm.h`): việc clear này resume đúng `RAILWAY_PREEMPTION`
(không phải luôn `NORMAL_OPERATION`) nếu crossing kề bên vẫn chưa
`CROSSING_OPEN` tại thời điểm clear - xem TC-FAULT-14b/14c bên dưới.

### TC-FAULT-11: Xác nhận không tồn tại phím trigger fault trực tiếp cho Lx
- **Loại**: Edge case (structural) - **chỉ review code, không có bước
  runtime nào để thực hiện qua bàn phím**
- **Liên quan**: PA-10, SC-03A
- **Môi trường**: N/A (code review)
- **Các bước**: Đọc toàn bộ `switch (input)` trong
  `lx_sensor_reader_thread()` (`lx_sensor.c`) và toàn bộ `lx_fsm.h`/
  `lx_fsm.c` để tìm mọi lời gọi tới `lx_fsm_report_watchdog_trip()` hoặc
  bất kỳ setter nào set `fsm->faults`.
- **Kết quả mong đợi**: Chỉ có đúng 1 nơi set `fsm->faults` là
  `lx_fsm_report_watchdog_trip()` (dòng `fsm->faults |= FAULT_WATCHDOG_TRIP;`
  trong `lx_fsm.c`), và hàm này chỉ có đúng 1 caller là
  `lx_watchdog_thread()`. Kết luận cần ghi vào biên bản test: **"Lx
  không có cách trigger FAULT_SAFE qua bàn phím trong bản hiện tại - chỉ
  verify được qua đọc code, không trigger được qua UI"**, đúng như yêu
  cầu trung thực của tài liệu này.

### TC-FAULT-12: FAULT_SAFE luôn thắng, NACK mọi lệnh mới với FAULT_ACTIVE (review + runtime có điều kiện)
- **Loại**: Positive, phụ thuộc điều kiện
- **Liên quan**: SC-03A, PA-09
- **Môi trường**: (B), phụ thuộc việc trip watchdog thành công (Nhóm 5,
  TC-FAULT-16/17)
- **Chuẩn bị**: Nếu TC-FAULT-16 (ép watchdog Lx trip qua debugger) thành
  công, dùng đúng con Lx đó. Nếu không, **bỏ qua phần runtime, chỉ verify
  bằng review code**.
- **Các bước (nếu trip được)**: Sau khi Lx vào `SUPERVISORY_FAULT_SAFE`,
  từ Central gửi lần lượt `m` (SET_MODE), `t` (SET_TIMING_PROFILE),
  `o` (REQUEST_OVERRIDE) tới đúng Lx đó.
- **Kết quả mong đợi**: Mọi lệnh đều nhận `RESULT_NACK`/
  `NACK_REASON_FAULT_ACTIVE` - vì mọi hàm `lx_fsm_on_*()` đều gọi
  `lx_fsm_check_fault_locked(fsm)` ngay đầu tiên và kiểm tra
  `fsm->supervisory == SUPERVISORY_FAULT_SAFE` trước khi xử lý verb.
  **Review code (luôn thực hiện được, không điều kiện)**: xác nhận cả 5
  handler (`lx_fsm_on_set_timing_profile`, `lx_fsm_on_set_mode`,
  `lx_fsm_on_request_override`, `lx_fsm_on_crossing_status` - verb này
  đặc biệt luôn ACK dù đang fault vì Lx chỉ quan sát, không dùng để ra
  lệnh) đều có nhánh `if (fsm->supervisory == SUPERVISORY_FAULT_SAFE)`
  đúng vị trí.

### TC-FAULT-13: FAULT_SAFE buộc terminate override đang chạy (SC-03A)
- **Loại**: Positive, phụ thuộc điều kiện - **tương tác đúng như yêu cầu
  kịch bản 6 của đề bài**
- **Liên quan**: SC-03A, PA-10
- **Môi trường**: (B), phụ thuộc TC-FAULT-16
- **Chuẩn bị**: Từ Central, cấp một `REQUEST_OVERRIDE` (phím `o`) cho Lx
  mục tiêu với `duration_ms` đủ dài (ví dụ 60000), xác nhận
  `override_active = 1` trong STATUS/HEARTBEAT hiển thị ở console
  Central (`c_hmi.c`'s bảng trạng thái, cột `OVERRIDE`).
- **Các bước**: Trong lúc override đang `OVR_ACTIVE`, ép watchdog trip
  con Lx đó (xem TC-FAULT-16).
- **Kết quả mong đợi**: `lx_fsm_report_watchdog_trip()` thấy
  `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE` -> gọi
  `lx_fsm_terminate_override_locked(fsm)` (đưa `override_substate`
  về `OVR_NONE`, `override_remaining_ms = 0`) TRƯỚC KHI set
  `fsm->supervisory = SUPERVISORY_FAULT_SAFE`. Trong lần STATUS/
  HEARTBEAT kế tiếp gửi về Central, cột `OVERRIDE` phải chuyển về 0 và
  cột `SUPERVISORY` hiển thị `FAULT_SAFE` - override không bao giờ "tự
  tiếp tục" sau khi fault clear (đúng nguyên tắc "never automatically
  resumes").
  **Nếu không trip watchdog được qua runtime**: verify bằng review code
  dòng 470-475 của `lx_fsm.c` (`lx_fsm_report_watchdog_trip()`) gọi
  `lx_fsm_terminate_override_locked()` khi
  `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE`, ghi rõ
  "verified by code review only".

### TC-FAULT-14: FAULT_SAFE không ảnh hưởng phản hồi cho `MSG_CROSSING_STATUS` (Lx chỉ quan sát, không chặn)
- **Loại**: Edge case / Negative nhẹ
- **Liên quan**: RC-02, SC-03A
- **Môi trường**: N/A (code review; runtime yêu cầu vừa trip watchdog Lx
  vừa có RLx thật gửi crossing status - tổ hợp phức tạp, không bắt buộc)
- **Các bước**: Đọc `lx_fsm_on_crossing_status()` trong `lx_fsm.c`.
- **Kết quả mong đợi**: Hàm luôn trả `RESULT_ACK` bất kể
  `fsm->supervisory` là gì (kể cả `FAULT_SAFE`) - đúng comment "RC-02: Lx
  only ever observes crossing status, it never rejects it" - nhưng khi
  đang `FAULT_SAFE`, cập nhật đó KHÔNG làm thay đổi `fsm->supervisory`
  (không thoát khỏi FAULT_SAFE, không vào RAILWAY_PREEMPTION). Đây là
  hành vi đúng theo thiết kế, không phải bug - ghi nhận qua review code.

### TC-FAULT-14b: REQUEST_FAULT_CLEAR đưa Lx thoát FAULT_SAFE về NORMAL_OPERATION (đã sửa - không còn known gap)
- **Loại**: Positive
- **Liên quan**: SC-03A, `lx_fsm_on_request_fault_clear()` (`lx_fsm.c`)
- **Môi trường**: (B), phụ thuộc TC-FAULT-16/17 để trip watchdog Lx trước.
- **Chuẩn bị**: L1 đang `SUPERVISORY_FAULT_SAFE` (watchdog trip qua Nhóm 5),
  và không có RLx kề nào đang pre-empt (`last_crossing_state ==
  CROSSING_OPEN`, ví dụ chưa từng nhận `CROSSING_STATUS` non-OPEN, hoặc
  RL1 đã báo lại `OPEN` trước khi trip).
- **Các bước**: Tại C1: `f` -> node type `0` (Lx) -> Lx number `1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1 ->
  ACK`. `fsm->faults` về `FAULT_NONE`, SUPERVISORY L1 chuyển `FAULT_SAFE
  -> NORMAL_OPERATION`. Không có nhánh NACK nào cho verb này ở phía Lx
  (khác RLx) - luôn ACK, kể cả khi gọi lại lần nữa lúc đã hết fault
  (idempotent).

### TC-FAULT-14c: REQUEST_FAULT_CLEAR trong lúc crossing kề bên vẫn đóng - resume RAILWAY_PREEMPTION, không phải NORMAL_OPERATION (re-audit fix, an toàn)
- **Loại**: Positive (safety-relevant regression case)
- **Liên quan**: SC-03A, CC-02, `last_crossing_state` (`lx_fsm.h`) - trước
  bản vá này, `lx_fsm_on_request_fault_clear()` luôn resume
  `NORMAL_OPERATION` vô điều kiện, có thể cho phép green hướng về một
  crossing vẫn còn đóng nếu fault xảy ra (hoặc còn active) trong lúc
  railway pre-emption đang suppress intersection đó.
- **Môi trường**: (B), cần cả `rlx_main` kề và watchdog trip được ở Lx
  (phụ thuộc Nhóm 5).
- **Chuẩn bị**: Đưa L1 vào `RAILWAY_PREEMPTION` thật (RL1 kề L1 ở
  WARNING/CLOSED, gửi `CROSSING_STATUS` khác `CROSSING_OPEN`), rồi trip
  watchdog Lx trong khi vẫn đang pre-empt (SUPERVISORY chuyển thẳng
  `RAILWAY_PREEMPTION -> FAULT_SAFE`; `lx_fsm_on_crossing_status()` vẫn
  cập nhật `fsm->last_crossing_state` vô điều kiện dù đang `FAULT_SAFE` -
  xem TC-FAULT-14 ở trên).
- **Các bước**: Tại C1: `f` -> `0` -> `1`, **trước khi** RL1 kịp báo
  `OPEN` trở lại.
- **Kết quả mong đợi**: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`, nhưng
  SUPERVISORY L1 sau đó phải là `RAILWAY_PREEMPTION`, **không phải**
  `NORMAL_OPERATION` - CONNECTOR_GREEN (hướng crossing) vẫn bị suppress
  cho tới khi L1 thực sự nhận `CROSSING_STATUS(OPEN)` từ RL1.
  **Nếu không trip watchdog được qua runtime**: verify bằng review code
  `lx_fsm_on_request_fault_clear()` trong `lx_fsm.c` (nhánh
  `fsm->last_crossing_state != CROSSING_OPEN ? SUPERVISORY_RAILWAY_
  PREEMPTION : SUPERVISORY_NORMAL_OPERATION`), ghi rõ "verified by code
  review only".

---

## Nhóm 5 - Watchdog trip (PA-10)

Đây là nhóm khó test runtime nhất trong toàn bộ tài liệu vì bản chất
watchdog là dead-man's-switch: để trigger thật, phải làm cho **thread xử
lý tick chính** (server thread) của tiến trình ngừng phản hồi trong khi
**thread watchdog riêng** (một thread độc lập trong cùng tiến trình) vẫn
tiếp tục chạy `sleep()`/so sánh bộ đếm. Hai thread này chạy trong CÙNG
một tiến trình, nên bất kỳ cách "treo cả tiến trình" nào (kể cả
`kill -STOP`) đều dừng luôn cả thread watchdog, khiến nó không bao giờ có
cơ hội phát hiện ra gì cả.

### TC-FAULT-15: Xác nhận logic watchdog qua review code (luôn thực hiện được, dùng làm baseline)
- **Loại**: Positive - review code, không phải runtime
- **Liên quan**: PA-10
- **Môi trường**: N/A (đọc code)
- **Các bước**: Đọc `lx_watchdog_thread()` (`lx_watchdog.c`) và
  `rlx_watchdog_thread()` (`rlx_watchdog.c`).
- **Kết quả mong đợi**:
  - Cả hai đều chạy vòng lặp `for(;;) { sleep(N); so sánh bộ đếm; }` với
    N = 2 s (Lx) / 3 s (RLx), độc lập, chạy trên thread riêng (không
    dùng chung thread với server/client - khớp với comment "PA-10 dead
    man's switch thread ... alongside the server, client, and sensor
    threads").
  - Khi bộ đếm không đổi giữa 2 lần kiểm tra liên tiếp, gọi
    `lx_fsm_report_watchdog_trip(args->fsm)` /
    `rlx_fsm_report_watchdog_trip(args->fsm)` - các hàm này tự lấy lock
    và tự set trạng thái FAULT ngay trong thread watchdog, **không phụ
    thuộc vào server thread còn sống hay không** (đúng như comment
    trong `lx_fsm.c`: "this function is called FROM the watchdog thread
    itself ... independent of whether the server thread ever runs
    another event again").
  - Đây là test case duy nhất trong Nhóm 5 **luôn PASS được, không cần
    máy QNX** - dùng làm bằng chứng tối thiểu bắt buộc phải có trước khi
    thử các test runtime bên dưới.

### TC-FAULT-16: Phản chứng - `kill -STOP` cả tiến trình KHÔNG trigger được watchdog
- **Loại**: Negative (chủ động phủ định một cách tiếp cận tưởng chừng
  hợp lý, để nhóm không tốn thời gian thử lại)
- **Liên quan**: PA-10
- **Môi trường**: (A), máy QNX thật, cần biết PID của `rlx_main`/`lx_main`
  (ví dụ qua `pidin` trên QNX)
- **Các bước**:
  1. Chạy `rlx_main 1`, ghi lại PID (`pidin | grep rlx_main`).
  2. `kill -STOP <pid>`, đợi 10 s (dài hơn nhiều so với 3 s check
     interval), rồi `kill -CONT <pid>`.
- **Kết quả mong đợi**: **KHÔNG** có fault nào được raise, không có log
  "WATCHDOG - no tick activity...". Lý do (ghi vào biên bản để giải
  thích, không phải vì test "fail"): `SIGSTOP`/`SIGCONT` theo ngữ nghĩa
  POSIX tác động lên toàn bộ tiến trình (mọi thread), không riêng một
  thread. Thread watchdog cũng bị dừng cùng lúc, nên khi cả hai thread
  cùng tỉnh dậy, bộ đếm `tick_counter` vẫn "vừa mới" tăng ở giá trị mà
  watchdog kỳ vọng (vì thời gian trôi qua trong lúc STOP không được
  thread nào cảm nhận là "khoảng lặng"). Kết luận: **`kill -STOP` không
  phải là cách hợp lệ để test PA-10** trong kiến trúc multi-thread một
  tiến trình này - cần một cách chỉ chặn riêng server thread (xem
  TC-FAULT-17/18).

### TC-FAULT-17: Best-effort - ép watchdog Lx trip bằng debugger (chặn riêng server thread)
- **Loại**: Positive - best effort, phụ thuộc công cụ, có thể không tái
  lập được 100% trên mọi cấu hình
- **Liên quan**: PA-10
- **Môi trường**: (A) hoặc (B), cần gdb/QNX Momentics debugger hỗ trợ
  điều khiển theo từng thread (non-stop mode hoặc tương đương) đính kèm
  vào `lx_main`
- **Chuẩn bị**: Chạy `lx_main 1` (L1), build có debug symbol.
- **Các bước**:
  1. Đính kèm debugger vào tiến trình `lx_main`.
  2. Đặt breakpoint bên trong hàm xử lý pulse chính của server thread
     (ví dụ đầu `lx_fsm_on_phase_timer()` trong `lx_fsm.c`, hoặc trong
     vòng lặp `on_pulse()`/dispatch của `lx_main.c`).
  3. Khi breakpoint được chạm, **chỉ giữ server thread đứng yên**; nếu
     debugger hỗ trợ, dùng chế độ "non-stop"/"scheduler-locking off cho
     riêng thread khác" để các thread còn lại (đặc biệt là thread chạy
     `lx_watchdog_thread()`) tiếp tục chạy bình thường.
  4. Giữ nguyên trạng thái đó tối thiểu 4 s (2 chu kỳ kiểm tra 2 s) rồi
     mới `continue`/thả breakpoint.
- **Kết quả mong đợi**: Sau khi resume, stderr của `lx_main` in ra
  `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)`
  và `lx_fsm_report_watchdog_trip()` được gọi ->
  `fsm->supervisory == SUPERVISORY_FAULT_SAFE`,
  `fsm->faults & FAULT_WATCHDOG_TRIP` != 0.
- **Ghi chú trung thực**: nhiều bản build gdb/IDE mặc định dừng TOÀN BỘ
  các thread khi breakpoint được chạm (kể cả thread watchdog), giống hệt
  vấn đề của TC-FAULT-16. Nếu công cụ debug sẵn có trên máy QNX của
  nhóm KHÔNG hỗ trợ giữ 1 thread dừng trong khi các thread khác chạy
  tiếp, test case này **không thể thực hiện đáng tin cậy qua runtime** -
  khi đó chỉ ghi nhận kết quả của TC-FAULT-15 (review code) làm bằng
  chứng thay thế, không cố gán ép một kết quả runtime không chắc chắn.

### TC-FAULT-18: Best-effort - ép watchdog RLx trip bằng debugger (tương tự TC-FAULT-17, ngưỡng 3 s)
- **Loại**: Positive - best effort, cùng giới hạn công cụ như TC-FAULT-17
- **Liên quan**: PA-10, RC-10 (gate phải bị đóng khi fault)
- **Môi trường**: (A) hoặc (B), cùng yêu cầu debugger như TC-FAULT-17
- **Các bước**: Tương tự TC-FAULT-17 nhưng đính kèm vào `rlx_main`, đặt
  breakpoint trong `rlx_fsm_on_tick()`, giữ tối thiểu 6 s (2 chu kỳ 3 s),
  chỉ dừng server thread, để thread `rlx_watchdog_thread()` chạy tiếp.
- **Kết quả mong đợi**: stderr in
  `RLx: WATCHDOG - no tick activity for 3 s, reporting fault (PA-10)`,
  `rlx_fsm_report_watchdog_trip()` gọi `enter_fault(FAULT_WATCHDOG_TRIP)`
  (vì `fsm->state != RLX_FAULT` trước đó) -> gate bị lệnh đóng ngay
  (`rlx_gate_command_close()`) dù trước đó có thể đang `RLX_OPEN` và
  hoàn toàn không có motion nào đang chạy - đây là điểm khác biệt quan
  trọng so với TC-FAULT-04 (nơi lỗi xảy ra giữa một motion OPENING đang
  chạy dở): ở đây lỗi xảy ra khi crossing đang nghỉ hoàn toàn ở `RLX_OPEN`,
  chứng minh `enter_fault()` chủ động đóng gate chứ không chỉ "sửa" một
  motion đang lỡ dở.
- **Ghi chú trung thực**: áp dụng cùng caveat về công cụ debug như
  TC-FAULT-17. Nếu không khả thi, dùng TC-FAULT-15 làm bằng chứng thay thế.

---

## Nhóm 6 - Stuck-sensor (giới hạn đã biết, được chấp nhận - không phải bug)

Theo comment trong chính source code, việc phát hiện cảm biến "kẹt ở
trạng thái active" (nút bấm người đi bộ giữ liên tục, cảm biến xe kẹt,
cảm biến tàu kẹt) **chưa được cài đặt** trong bản PoC hiện tại - đây là
giới hạn được nhóm chấp nhận từ trước (documented future work), không
phải lỗi cần fix. Mục đích của các test case dưới đây là **xác nhận hành
vi hiện tại đúng như tài liệu mô tả** (tức "không phát hiện" đúng như đã
biết), để tránh nhầm lẫn "không phát hiện" là một bug mới trong lúc kiểm
thử toàn hệ thống.

### TC-FAULT-19: Nút bấm người đi bộ "kẹt" (giữ/bấm liên tục) không set FAULT_PED_BUTTON_STUCK
- **Loại**: Negative (xác nhận giới hạn được chấp nhận)
- **Liên quan**: PA-03
- **Môi trường**: (A)
- **Chuẩn bị**: `lx_main 1`.
- **Các bước**: Bấm phím `1` (pedestrian button, side 0) liên tục nhiều
  lần trong một khoảng thời gian dài (ví dụ mỗi 1 s trong 2 phút, mô
  phỏng nút bị kẹt/giữ vật lý liên tục).
- **Kết quả mong đợi**: `fsm->ped_latched[0]` vẫn được set/giữ đúng theo
  logic coalescing bình thường của `lx_fsm_latch_pedestrian_request()`
  (không crash, không loop vô hạn), NHƯNG `fsm->faults` **không bao
  giờ** có bit `FAULT_PED_BUTTON_STUCK` được set, dù giữ bao lâu đi nữa -
  đúng như comment "there is no 'stuck active beyond a diagnostic
  timeout' detection here, so FAULT_PED_BUTTON_STUCK ... is never set by
  this file" trong `lx_fsm.c`. Đây là kết quả **đạt yêu cầu** (pass),
  không phải lỗi cần báo cáo.

### TC-FAULT-20: Cảm biến xe "kẹt" (giữ demand liên tục) không set FAULT_VEHICLE_SENSOR_STUCK
- **Loại**: Negative (xác nhận giới hạn được chấp nhận)
- **Liên quan**: PA-06
- **Môi trường**: (A)
- **Các bước**: Bấm `a` (arterial vehicle present) và không bao giờ bấm
  `A` (clear) trong suốt một khoảng thời gian dài (nhiều phút, trải qua
  nhiều chu kỳ pha).
- **Kết quả mong đợi**: `fsm->arterial_vehicle_demand` giữ nguyên 1 (mô
  phỏng đúng hành vi của một cảm biến thực sự kẹt), hệ thống vẫn hoạt
  động bình thường (ở `MODE_OFF_PEAK_SENSOR`, làn arterial sẽ liên tục
  được coi là có nhu cầu, có thể kéo dài phiên xanh nhiều hơn thực tế
  nhưng không bao giờ vượt `LX_MAX_GREEN_MS`), nhưng
  `fsm->faults` không có bit `FAULT_VEHICLE_SENSOR_STUCK` - đúng như
  thiết kế hiện tại (không có cơ chế đo "đã active bao lâu" cho demand
  này). Ghi nhận là **giới hạn chấp nhận được**, không phải bug.

### TC-FAULT-21: Cảm biến tàu "kẹt" (TRAIN_APPROACHING liên tục) không set FAULT_TRAIN_SENSOR_STUCK
- **Loại**: Negative (xác nhận giới hạn được chấp nhận)
- **Liên quan**: RC-11
- **Môi trường**: (A)
- **Các bước**: Trong lúc RL1 đang ở `RLX_WARNING`, bấm `0` lặp lại
  nhiều lần trước khi đủ 5 s (`RLX_WARNING_TO_CLOSING_MS`) trôi qua, mô
  phỏng một cảm biến báo tàu tới bị kẹt (liên tục xác nhận "có tàu" dù
  chưa chắc còn tàu thật).
- **Kết quả mong đợi**: Theo `rlx_fsm_simulate_train_approaching()`, case
  `RLX_WARNING` chỉ gọi `register_window()` lại (self-loop), không reset
  lại timer đóng gate và không set fault nào. Theo đúng comment trong
  `rlx_fsm_on_tick()`'s case `RLX_WARNING`: hằng số
  `RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS` (60 s) tồn tại trong
  `rlx_timer.h` nhưng **RESERVED, không được tham chiếu ở đâu cả** -
  logic "stuck active" thật sự cần một đường tín hiệu cảm biến liên tục
  (continuous sensor line) mà sự kiện rời rạc qua phím bấm không thể mô
  phỏng được. Do đó, `fsm->faults` sẽ **không bao giờ** có bit
  `FAULT_TRAIN_SENSOR_STUCK` được set trong bản hiện tại, bất kể bấm `0`
  bao nhiêu lần hay trong bao lâu. Đây là giới hạn đã được chính đội ngũ
  phát triển ghi chú rõ trong code (không phải phát hiện mới), verify
  bằng cả review code lẫn quan sát runtime đều cho cùng kết luận.

---

## Tổng kết phạm vi có thể test runtime trên máy QNX thật

| Nhóm | Test runtime được (không cần debugger) | Cần debugger | Chỉ review code |
|---|---|---|---|
| 1. RC-06 gate confirm | TC-01, TC-02, TC-03 | - | - |
| 2. Fault ép đóng gate | TC-04 | - | - |
| 3. REQUEST_FAULT_CLEAR | TC-05, TC-06, TC-08, TC-09 | TC-07, TC-10 | - |
| 4. FAULT_SAFE tại Lx | TC-14b (phụ thuộc Nhóm 5 để trip trước) | TC-12, TC-13, TC-14c (phụ thuộc Nhóm 5) | TC-11, TC-14 |
| 5. Watchdog trip | (phủ định) TC-16 | TC-17, TC-18 | TC-15 |
| 6. Stuck-sensor | TC-19, TC-20, TC-21 | - | - |

23 test case (bổ sung TC-FAULT-14b/14c sau khi `MSG_REQUEST_FAULT_CLEAR`
được nối dây cho Lx), phần lớn (14/23) chạy được hoàn toàn qua bàn phím
trên máy QNX thật không cần công cụ gì thêm; 6 test case cần debugger
(ghi rõ caveat nếu không khả thi); 2 test case là review-code thuần túy
do bản chất kiến trúc hiện tại (Lx không có phím trigger fault) không
cho phép runtime; 1 test case (TC-16) là phản chứng có chủ đích.
