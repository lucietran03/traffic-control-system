# 03 - Test Plan: Hợp đồng giao thức IPC (Protocol Contract)

Phạm vi tài liệu này: kiểm thử từng **verb** trong `msg_type_t`
(`app/shared/includes/ipc_msg.h`) và từng **outcome** có thể xảy ra khi
verb đó được gửi qua Qnet (`RESULT_ACK` / `RESULT_ACK_PENDING` /
`RESULT_NACK` + `nack_reason_t` cụ thể / `RESULT_ERROR`), đối chiếu trực
tiếp với logic thật trong `lx_fsm.c`, `rlx_fsm.c`, `c_mode_eng.c`,
`c_main.c`. Đây KHÔNG phải test plan chức năng/timing (xem các file khác
trong `docs/test-plan/`) - trọng tâm duy nhất ở đây là: "với input X, bộ
đôi (result, reason) trả về trên dây có đúng như code quy định không?".

## Quy ước môi trường (A/B/C/D)

| Ký hiệu | Ý nghĩa |
|---|---|
| **(A)** | 1 node đơn, không cần Qnet thật (ví dụ: chỉ chạy `c_main`/`lx_main` một mình, quan sát hành vi nội tại - hiếm dùng cho test giao thức vì cần ít nhất 2 phía gửi/nhận). |
| **(B)** | Nhiều tiến trình (node) chạy **cùng một máy QNX**, mỗi node là 1 executable riêng (`c_main`, `lx_main <n>`, `rlx_main <n>`), giao tiếp qua Qnet nội bộ (không cần `TRAFFIC_NODE_MAP`, mặc định "same node as caller" - xem `qnet_utils.h`). Đủ để kiểm tra đúng-sai của hợp đồng giao thức vì `MsgSend/MsgReceive/MsgReply` vẫn đi qua kernel thật. |
| **(C)** | Nhiều **máy/VM QNX vật lý hoặc ảo** khác nhau, nối mạng thật, dùng biến môi trường `TRAFFIC_NODE_MAP` để phân giải node name (xem `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`). Dùng cho các test cần xác nhận hành vi không đổi khi qua mạng thật (độ trễ, mất gói...). Với test hợp đồng giao thức thuần túy, (B) và (C) cho kết quả **result/reason giống hệt nhau** - tài liệu này dùng (C) chỉ khi cần nhấn mạnh yêu cầu "phải chạy trên môi trường phân tán thật" của đồ án. |
| **(D)** | **Cần công cụ test_client riêng (CHƯA CÓ trong repo - đề xuất xây dựng).** `c_operator.c` (bàn phím tại C1) và `lx_sensor.c`/`rlx_sensor.c` (bàn phím tại Lx/RLx) chỉ cho phép nhập giá trị hợp lệ về hình thức (chúng hỏi số qua `scanf`, không có đường nào để tự gửi payload sai kiểu, verb lạ, hoặc nhắm sai loại node). Một số outcome trong hợp đồng giao thức chỉ xảy ra khi có message "không đúng luật chơi bình thường" - các case đó bắt buộc môi trường (D). |

### Đề xuất công cụ test_client (cho mọi case đánh dấu (D))

Chưa tồn tại trong repo. Đề xuất: 1 executable nhỏ,
`app/tools/test_client/test_client.c`, dùng lại thẳng
`app/shared/includes/qnet_utils.h` / `qnet_utils.c` (đã có sẵn
`ipc_attach_name()` để dựng tên attach point và cơ chế `TRAFFIC_NODE_MAP`
để mở kết nối tới node ở xa) và `ipc_msg.h`. Không cần dùng
`ipc_client_queue_t`/luồng nền như các node thật - chỉ cần:

```c
name_open("/net/<node>/dev/name/global/traffic/<suffix>", 0) (hoặc same-node)
ipc_request_t req; /* tự tay set moi field, kể cả field "không hợp lệ" */
MsgSend(coid, &req, sizeof(req), &reply, sizeof(reply));
in ra reply.result / reply.reason
```

Vì `ipc_request_t`/`ipc_reply_t` và mọi hằng số verb/nack_reason đã là
public header dùng chung, test_client chỉ cần ~100 dòng: parse tham số
dòng lệnh (target node, verb, các field payload dạng số), build
`ipc_request_t` union theo đúng verb, gửi 1 lần, in reply. Đây là công cụ
"whitebox" duy nhất có thể tạo ra: (1) payload hợp lệ nhưng giá trị nằm
ngoài mọi biên số mà `c_operator.c` cho phép nhập, (2) verb gửi sai loại
node, (3) 2 request gửi gần như đồng thời từ 2 tiến trình test_client
độc lập để dò race, (4) payload cố tình sai định dạng (chuỗi không kết
thúc `\0`, enum ngoài phạm vi).

### Quy ước chung khác

- Log cần xem: `central_log.txt` (ghi bởi `c_logger_log()`, đồng thời in
  ra stdout của tiến trình `c_main`) theo đúng định dạng của
  `c_comm.c`'s `on_command_reply()`:
  - NACK: `C1: <VERB> to <target> -> NACK reason=<REASON>`
  - Khác: `C1: <VERB> to <target> -> <ACK|ACK_PENDING|ERROR>`
  Với các verb Lx/RLx tự gửi đến C1 (STATUS/HEARTBEAT/FAULT_REPORT/
  CROSSING_STATUS), log tương ứng nằm ở nhánh `on_request()` trong
  `c_main.c` (một số nhánh hiện không log gì ngoài cập nhật
  `c_mode_eng_t` - xem ghi chú trong từng test case).
- Lx: chạy `lx_main <1..6>`; RLx: chạy `rlx_main <1..3>`; C1: chạy
  `c_main`. Phím điều khiển: xem `print_help()` trong `c_operator.c` /
  `lx_sensor.c` / `rlx_sensor.c`.
- Bản đồ kề cận railway-intersection (`rlx_comm.c` `ADJACENCY[]`):
  RL1 kề L1,L2; RL2 kề L3,L4; RL3 kề L5,L6.
- Hằng số quan trọng: `LX_CYCLE_LENGTH_MS = 90000` (48000+4000+2000+
  30000+4000+2000, `lx_timer.h`), `LX_OVERRIDE_DURATION_CAP_MS =
  300000`, offset chuỗi R1 = {L1:0, L3:21000, L5:45000}ms, R2 =
  {L2:0, L4:19000, L6:42000}ms (`c_mode_eng.h`), `RLX_GATE_MOTION_MS =
  3000`, `RLX_WARNING_TO_CLOSING_MS = 5000`,
  `RLX_CLOSING_DEADLINE_MS = 15000`, `RLX_OPENING_DEADLINE_MS = 15000`
  (`rlx_timer.h`/`rlx_gate.h`).
- **Phát hiện quan trọng cần biết trước khi test (chi tiết ở Phụ lục
  cuối file):** (1) `lx_sensor.c` không có phím kích hoạt lỗi thủ công
  nào (khác với `rlx_sensor.c` đã có phím `x`/`f`) nên không có cách nào
  ép một Lx vào `SUPERVISORY_FAULT_SAFE` chỉ bằng bàn phím - PA-10 chỉ
  kích hoạt thật khi luồng server bị treo >= 2s thật sự; (2)
  `MSG_STATUS` được `c_main.c` xử lý nhưng **không có bất kỳ nơi nào
  trong code hiện tại thực sự gửi** verb này (`lx_comm.c`/`rlx_comm.c`
  chỉ gửi HEARTBEAT/FAULT_REPORT/CROSSING_STATUS); (3) `RESULT_ACK` của
  `MSG_REQUEST_FAULT_CLEAR` có vẻ **không thể đạt được** với code hiện
  tại vì không có đường nào lệnh mở cổng (`rlx_gate_command_open()`)
  chạy khi đang ở `RLX_FAULT`; (4) `NACK_REASON_PEDESTRIAN_ACTIVE` được
  khai báo và có tên log nhưng **không được gán ở bất kỳ đâu** trong
  `lx_fsm.c` (case ped-clearance dùng `ACK_PENDING`, không dùng NACK
  này) - xem Phụ lục.

---

## 1. MSG_SET_TIMING_PROFILE (C1 -> Lx)

Xử lý bởi `lx_fsm_on_set_timing_profile()`. NACK duy nhất 2 nhánh:
`NACK_REASON_FAULT_ACTIVE` (đang FAULT_SAFE) và
`NACK_REASON_STALE_OR_UNSAFE_PROFILE` (`offset_ms >= LX_CYCLE_LENGTH_MS`
= 90000). `c_operator.c`'s phím `t` chỉ gửi offset cố định từ
`R1_CHAIN`/`R2_CHAIN` (0/21000/45000/19000/42000ms) - không có offset
nào >= 90000, nên case biên phải dùng test_client.

### TC-MSG-1: SET_TIMING_PROFILE hợp lệ - ACK
- **Loại**: Positive
- **Verb**: MSG_SET_TIMING_PROFILE
- **Liên quan**: (không có nack)
- **Môi trường**: (B) C1 + L1 (+ L3, L5 cùng chuỗi R1, không bắt buộc nhưng nên chạy đủ để broadcast không lỗi)
- **Chuẩn bị**: Khởi động `c_main`, `lx_main 1`, `lx_main 3`, `lx_main 5`.
- **Các bước**: Tại C1: gõ `t` -> `chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): 1`.
- **Kết quả mong đợi**: `central_log.txt` có 3 dòng `C1: SET_TIMING_PROFILE to <id> -> ACK` (id = 1, 3, 5 - CTRL_L1/L3/L5). Tại L1: `active_profile_id` cập nhật = profile_id vừa cấp (in trong log nội bộ nếu có), `offset_apply_pending=1` (quan sát gián tiếp: green-wave dịch đúng offset ở chu kỳ arterial-green kế tiếp).

### TC-MSG-2: SET_TIMING_PROFILE khi Lx đang FAULT_SAFE - NACK FAULT_ACTIVE
- **Loại**: Negative
- **Verb**: MSG_SET_TIMING_PROFILE
- **Liên quan**: NACK_REASON_FAULT_ACTIVE
- **Môi trường**: (D) cần công cụ hỗ trợ ép fault. `lx_sensor.c` hiện KHÔNG có phím lỗi thủ công (khác `rlx_sensor.c`'s `x`/`f`), và PA-10 chỉ thật sự trip khi luồng server-thread của Lx ngừng tick >= 2s liên tục (`lx_watchdog.c`, `LX_WATCHDOG_CHECK_INTERVAL_S=2`) - không có cách kích hoạt xác định bằng thao tác bàn phím/CLI thông thường. Đề xuất: bổ sung tạm 1 phím DEMO-ONLY vào `lx_sensor.c` gọi thẳng `lx_fsm_report_watchdog_trip(&fsm)`, đúng khuôn mẫu đã có ở `rlx_sensor.c`'s phím `f`, để nhóm test có thể ép `SUPERVISORY_FAULT_SAFE` mà không cần chờ watchdog thật.
- **Chuẩn bị**: Sau khi có phím demo (hoặc gắn debugger tạm dừng thread server của `lx_main 1` bằng breakpoint > 2s), xác nhận qua log "Lx: WATCHDOG - no phase-timer activity ... reporting fault" xuất hiện.
- **Các bước**: Từ C1: `t` -> `1` (broadcast R1) trong khi L1 đang FAULT_SAFE.
- **Kết quả mong đợi**: `central_log.txt`: `C1: SET_TIMING_PROFILE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-3: offset_ms đúng ranh giới trên (>= LX_CYCLE_LENGTH_MS) - NACK STALE_OR_UNSAFE_PROFILE (edge case)
- **Loại**: Edge case
- **Verb**: MSG_SET_TIMING_PROFILE
- **Liên quan**: NACK_REASON_STALE_OR_UNSAFE_PROFILE; biên `offset_ms >= 90000` (`lx_fsm_on_set_timing_profile()`)
- **Môi trường**: (D) - `c_operator.c` không cho nhập offset tùy ý, chỉ gửi hằng số có sẵn trong `c_mode_eng.c`.
- **Chuẩn bị**: `c_main`, `lx_main 1` chạy sẵn, L1 ở trạng thái NORMAL_OPERATION (không fault).
- **Các bước**: test_client gửi trực tiếp tới L1 một `ipc_request_t{verb=MSG_SET_TIMING_PROFILE, sender_id=CTRL_C1, target_id=CTRL_L1, payload.timing_profile={profile_id=99, offset_ms=90000}}`.
- **Kết quả mong đợi**: reply `result=RESULT_NACK`, `reason=NACK_REASON_STALE_OR_UNSAFE_PROFILE`. `active_profile_id`/`assigned_offset_ms` của L1 KHÔNG đổi.

### TC-MSG-4: offset_ms ngay dưới ranh giới (LX_CYCLE_LENGTH_MS - 1) - ACK (edge case)
- **Loại**: Edge case
- **Verb**: MSG_SET_TIMING_PROFILE
- **Liên quan**: biên an toàn ngay dưới NACK_REASON_STALE_OR_UNSAFE_PROFILE
- **Môi trường**: (D)
- **Chuẩn bị**: Giống TC-MSG-3.
- **Các bước**: test_client gửi `payload.timing_profile={profile_id=100, offset_ms=89999}` tới L1.
- **Kết quả mong đợi**: reply `result=RESULT_ACK`. `active_profile_id=100`, `assigned_offset_ms=89999`, `offset_apply_pending=1` (áp dụng ở arterial-green kế tiếp).

---

## 2. MSG_SET_MODE (C1 -> Lx)

Xử lý bởi `lx_fsm_on_set_mode()`. 2 nhánh NACK
(`NACK_REASON_FAULT_ACTIVE`, và - re-audit fix, xem TC-MSG-8b -
`NACK_REASON_OUT_OF_RANGE` khi `payload->mode` không phải 0
(`MODE_PEAK_FIXED`) hay 1 (`MODE_OFF_PEAK_SENSOR`), đúng yêu cầu UC-07
main flow bước 3 "validates the request against supported ranges");
2 nhánh tích cực: `RESULT_ACK` (mode gửi == mode hiện tại, coi như
no-op) và `RESULT_ACK_PENDING` (mode khác, hoãn tới ranh giới ALL_RED
kế tiếp - SC-01A).

### TC-MSG-5: SET_MODE với mode trùng mode hiện tại - ACK (no-op)
- **Loại**: Positive
- **Verb**: MSG_SET_MODE
- **Liên quan**: (không có nack)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 khởi động ở `MODE_PEAK_FIXED` (mặc định cold-start, `lx_fsm_init()`).
- **Các bước**: Tại C1: `m` -> `Lx number: 1` -> `mode: 0` (PEAK_FIXED).
- **Kết quả mong đợi**: `central_log.txt`: `C1: SET_MODE to 1 -> ACK`. `mode_change_pending` tại L1 vẫn = 0 (không có gì bị hoãn).

### TC-MSG-6: SET_MODE sang mode khác - ACK_PENDING, áp dụng đúng tại ranh giới ALL_RED
- **Loại**: Positive
- **Verb**: MSG_SET_MODE
- **Liên quan**: (không có nack) - PA-... SC-01A
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 đang `MODE_PEAK_FIXED`, đang ở giữa `PHASE_ARTERIAL_GREEN` hoặc bất kỳ pha nào chưa tới ALL_RED.
- **Các bước**: Tại C1: `m` -> `1` -> `mode: 1` (OFF_PEAK_SENSOR). Theo dõi log/console L1 qua tối đa 1 chu kỳ (<= ~54s tới `PHASE_ALL_RED_A_TO_B` hoặc `PHASE_ALL_RED_B_TO_A` gần nhất).
- **Kết quả mong đợi**: Ngay lập tức: `central_log.txt`: `C1: SET_MODE to 1 -> ACK_PENDING`. Sau đó, đúng tại lần vào ALL_RED kế tiếp (không sớm hơn, không làm gián đoạn pha đang chạy), `fsm->mode` chuyển thành `MODE_OFF_PEAK_SENSOR` (quan sát qua hành vi extension 4s thay vì fixed-duration ở `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN` kế tiếp).

### TC-MSG-7: Hủy pending mode-change bằng cách gửi lại mode hiện tại trước ranh giới (edge case)
- **Loại**: Edge case
- **Verb**: MSG_SET_MODE
- **Liên quan**: hành vi `mode_change_pending=0` khi payload->mode == fsm->mode hiện tại (`lx_fsm_on_set_mode()`)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 đang `MODE_PEAK_FIXED`.
- **Các bước**: (1) `m` -> `1` -> `1` (yêu cầu OFF_PEAK_SENSOR) -> nhận ACK_PENDING. (2) Trước khi L1 chạm ranh giới ALL_RED kế tiếp, gửi tiếp `m` -> `1` -> `0` (yêu cầu lại PEAK_FIXED, tức mode hiện tại thật sự).
- **Kết quả mong đợi**: Bước (2) trả `result=RESULT_ACK` (không phải ACK_PENDING, vì `payload->mode == fsm->mode` hiện hành). `mode_change_pending` bị đặt lại về 0 - tại ranh giới ALL_RED kế tiếp, L1 KHÔNG đổi mode (vẫn PEAK_FIXED), xác nhận yêu cầu (1) đã bị hủy hoàn toàn chứ không âm thầm áp dụng.

### TC-MSG-8: SET_MODE khi Lx đang FAULT_SAFE - NACK FAULT_ACTIVE
- **Loại**: Negative
- **Verb**: MSG_SET_MODE
- **Liên quan**: NACK_REASON_FAULT_ACTIVE
- **Môi trường**: (D) - cùng lý do/đề xuất như TC-MSG-2 (cần phím demo ép fault trên `lx_sensor.c`, hiện chưa có).
- **Chuẩn bị**: L1 ở `SUPERVISORY_FAULT_SAFE`.
- **Các bước**: Tại C1: `m` -> `1` -> `1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: SET_MODE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-8b: SET_MODE với `mode` ngoài phạm vi hợp lệ (khác 0/1) - NACK OUT_OF_RANGE (UC-07 bước 3)
- **Loại**: Negative (re-audit fix)
- **Verb**: MSG_SET_MODE
- **Liên quan**: `NACK_REASON_OUT_OF_RANGE`, UC-07 main flow bước 3 ("validates
  the request against supported ranges"). `c_operator.c`'s `handle_set_mode()`
  đã tự chặn giá trị khác 0/1 ngay tại console (`n != MODE_PEAK_FIXED &&
  n != MODE_OFF_PEAK_SENSOR` -> "command aborted", không gửi gì đi) - nên
  nhánh NACK này **không thể tái hiện qua bàn phím `c_operator`**, chỉ qua
  test_client gửi thẳng một payload không hợp lệ, đúng như comment trong
  `lx_fsm_on_set_mode()`: "this FSM (not the console) is the documented
  authoritative validator - the wire contract has no guarantee the sender
  is always a well-behaved operator".
- **Môi trường**: (D) - bắt buộc, vì `c_operator.c`'s pre-check chặn y hệt
  ngưỡng này trước khi gửi.
- **Chuẩn bị**: L1 không có fault, đang `MODE_PEAK_FIXED`.
- **Các bước**: test_client gửi trực tiếp tới L1 một
  `ipc_request_t{verb=MSG_SET_MODE, sender_id=CTRL_C1, target_id=CTRL_L1,
  payload.mode={mode=2}}` (bất kỳ giá trị nào khác 0/1).
- **Kết quả mong đợi**: reply `result=RESULT_NACK`,
  `reason=NACK_REASON_OUT_OF_RANGE`; `fsm->mode`/`fsm->mode_change_pending`
  không đổi (yêu cầu bị từ chối hoàn toàn, không âm thầm rơi vào nhánh
  `else` như hành vi cũ trước re-audit fix).

---

## 3. MSG_REQUEST_OVERRIDE (C1 -> Lx)

Xử lý 2 tầng: (1) `c_mode_eng_validate_override_request()` tại **Central**
(pre-check hình thức: target hợp lệ, `duration_ms` trong (0, 300000],
`override_type == OVERRIDE_CLEAR_ROUTE`) - nếu fail, request **không hề
được gửi lên dây** (`c_operator.c`'s `handle_request_override()` chỉ log
nội bộ tại C1, KHÔNG có `ipc_reply_t` thật từ Lx); (2)
`lx_fsm_on_request_override()` tại **Lx** - tầng bảo vệ sâu hơn, thứ tự
kiểm tra chính xác trong code: đã có CENTRAL_OVERRIDE khác đang chạy ->
`NACK_REASON_OUT_OF_RANGE`; `duration_ms==0` hoặc `>300000` ->
`NACK_REASON_INVALID_DURATION`; đang `RAILWAY_PREEMPTION` ->
`NACK_REASON_RAILWAY_CONFLICT`; đang `FAULT_SAFE` ->
`NACK_REASON_FAULT_ACTIVE`; đang `ped_clearance_active` ->
`RESULT_ACK_PENDING` (KHÔNG NACK - xem Phụ lục về
`NACK_REASON_PEDESTRIAN_ACTIVE`); còn lại -> `RESULT_ACK`.

Vì ngưỡng duration ở Central (0, 300000] và ở Lx giống hệt nhau,
**`c_operator.c` không bao giờ có thể chạm tới nhánh
`NACK_REASON_INVALID_DURATION` thật sự nằm trong `lx_fsm.c`** - mọi
duration operator gõ ra ngoài (0,300000] đều đã bị Central chặn lại
trước khi gửi đi. Test case Lx-side phải dùng test_client gửi thẳng,
bỏ qua Central.

### TC-MSG-9: REQUEST_OVERRIDE hợp lệ, không xung đột - ACK
- **Loại**: Positive
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: (không có nack)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 ở NORMAL_OPERATION, không fault, không railway preemption, không ped clearance đang chạy.
- **Các bước**: C1: `o` -> `Lx number: 1` -> `target movement: 0` (arterial) -> `duration_ms: 30000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK`. L1: `supervisory=SUPERVISORY_CENTRAL_OVERRIDE`, `override_substate=OVR_ACTIVE`, giữ green ở movement ARTERIAL trong 30s rồi tự kết thúc (safe clearance).

### TC-MSG-10: REQUEST_OVERRIDE trong lúc ped clearance đang chạy - ACK_PENDING (SC-03B)
- **Loại**: Positive
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: (không có nack - đây chính là case mà comment code gợi ý "NACK_REASON_PEDESTRIAN_ACTIVE" nhưng thực tế trả ACK_PENDING, xem Phụ lục)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Tại L1's sensor console (`lx_sensor_reader_thread`), khi `PHASE_ARTERIAL_GREEN` đang chạy, bấm `1` (ped side 0) để bắt đầu chuỗi WALK/FLASHING_DONT_WALK (tổng 10s: 6s WALK + 4s FDW).
- **Các bước**: Ngay khi chuỗi WALK/FDW đang chạy (trong vòng 10s đó), tại C1: `o` -> `1` -> `target movement: 0` (arterial, cùng phía đang phục vụ) -> `duration_ms: 20000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING`. L1: `override_substate=OVR_PENDING_CLEARANCE`, `supervisory=SUPERVISORY_CENTRAL_OVERRIDE` ngay lập tức, nhưng override chỉ thật sự giữ green (OVR_ACTIVE) SAU KHI chuỗi WALK/FDW hoàn tất (`ped_clearance_active` về 0) - không có reply thứ hai nào được gửi khi override được kích hoạt thật.

### TC-MSG-11: REQUEST_OVERRIDE khi đã có override khác đang chạy - NACK OUT_OF_RANGE
- **Loại**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: NACK_REASON_OUT_OF_RANGE (dùng tạm vì "chưa có mã lỗi riêng cho trường hợp này" - comment trong `lx_fsm_on_request_override()`)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Thực hiện TC-MSG-9 trước (L1 đang có override ACTIVE, chưa hết hạn).
- **Các bước**: Trong lúc override đầu vẫn còn hiệu lực, gửi tiếp: C1: `o` -> `1` -> `target movement: 1` -> `duration_ms: 10000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=OUT_OF_RANGE`. Override đầu tiên (arterial, 30s) tiếp tục chạy không đổi.

### TC-MSG-12: REQUEST_OVERRIDE duration_ms=0 - NACK INVALID_DURATION (chặn tại Central pre-check)
- **Loại**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: NACK_REASON_INVALID_DURATION (`c_mode_eng_validate_override_request()`, KHÔNG phải reply thật từ Lx)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 chạy bình thường.
- **Các bước**: C1: `o` -> `1` -> `target movement: 0` -> `duration_ms: 0`.
- **Kết quả mong đợi**: `central_log.txt`: `Operator: REQUEST_OVERRIDE(target=1, movement=0, duration_ms=0) rejected by Central pre-check, reason=INVALID_DURATION - not forwarded to the controller`. **KHÔNG có** dòng `C1: REQUEST_OVERRIDE to 1 -> ...` nào (vì request chưa từng rời khỏi C1 - `ipc_client_post()` không được gọi).

### TC-MSG-13: REQUEST_OVERRIDE duration_ms=300001 - NACK INVALID_DURATION (chặn tại Central pre-check)
- **Loại**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: NACK_REASON_INVALID_DURATION (Central pre-check)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Giống TC-MSG-12.
- **Các bước**: C1: `o` -> `1` -> `0` -> `duration_ms: 300001`.
- **Kết quả mong đợi**: Giống TC-MSG-12 nhưng `duration_ms=300001` trong log; không có message gửi tới L1.

### TC-MSG-14: REQUEST_OVERRIDE duration_ms=0 gửi thẳng tới Lx, bỏ qua Central - NACK INVALID_DURATION (tầng Lx thật)
- **Loại**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: NACK_REASON_INVALID_DURATION (nhánh THẬT trong `lx_fsm_on_request_override()`, không qua `c_operator.c`)
- **Môi trường**: (D) - bắt buộc, vì `c_operator.c`'s pre-check chặn y hệt ngưỡng này trước khi gửi (xem phần đầu mục 3).
- **Chuẩn bị**: L1 chạy bình thường, không có override đang active.
- **Các bước**: test_client gửi trực tiếp tới L1: `ipc_request_t{verb=MSG_REQUEST_OVERRIDE, sender_id=CTRL_C1, target_id=CTRL_L1, payload.override_request={override_type=OVERRIDE_CLEAR_ROUTE, target_movement=0, duration_ms=0}}`.
- **Kết quả mong đợi**: reply `result=RESULT_NACK`, `reason=NACK_REASON_INVALID_DURATION`. Đây là bằng chứng độc lập rằng Lx có validation phòng thủ riêng (defense-in-depth), không chỉ dựa vào Central.

### TC-MSG-15: REQUEST_OVERRIDE khi Lx đang RAILWAY_PREEMPTION - NACK RAILWAY_CONFLICT
- **Loại**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: NACK_REASON_RAILWAY_CONFLICT
- **Môi trường**: (B)/(C) C1 + L1 + RL1 (L1 kề RL1 theo `ADJACENCY[]`)
- **Chuẩn bị**: Khởi động `rlx_main 1`. Tại RL1's sensor console, bấm `0` (TRAIN_APPROACHING direction 0) để RL1 chuyển OPEN -> WARNING, kích hoạt broadcast `MSG_CROSSING_STATUS(WARNING)` tới L1 (và L2, C1). Xác nhận L1 đã vào `SUPERVISORY_RAILWAY_PREEMPTION`.
- **Các bước**: C1: `o` -> `Lx number: 1` -> `target movement: 1` (connector - hướng bị railway chặn) -> `duration_ms: 10000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=RAILWAY_CONFLICT`.

### TC-MSG-16: REQUEST_OVERRIDE khi Lx đang FAULT_SAFE - NACK FAULT_ACTIVE
- **Loại**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: NACK_REASON_FAULT_ACTIVE
- **Môi trường**: (D) - cùng lý do TC-MSG-2 (cần phím demo ép fault, chưa có trong `lx_sensor.c`).
- **Chuẩn bị**: L1 ở `SUPERVISORY_FAULT_SAFE`.
- **Các bước**: C1: `o` -> `1` -> `0` -> `duration_ms: 10000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-17: duration_ms=1 - ACK (biên dưới hợp lệ, PA-11)
- **Loại**: Edge case
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: PA-11, biên `(0, 300000]`
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 bình thường, không override/fault/preemption.
- **Các bước**: C1: `o` -> `1` -> `target movement: 0` -> `duration_ms: 1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK`. Override kết thúc gần như ngay lập tức ở tick 100ms kế tiếp của `lx_fsm_on_phase_timer()` (vì `override_remaining_ms=1 <= LX_PHASE_TICK_MS=100`).

### TC-MSG-18: duration_ms=300000 - ACK (biên trên hợp lệ, đúng PA-11)
- **Loại**: Edge case
- **Verb**: MSG_REQUEST_OVERRIDE
- **Liên quan**: PA-11, biên trên `LX_OVERRIDE_DURATION_CAP_MS = 300000`
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Giống TC-MSG-17.
- **Các bước**: C1: `o` -> `1` -> `0` -> `duration_ms: 300000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK` (300000 chấp nhận được, 300001 đã bị NACK ở TC-MSG-13/tương đương tầng Lx). `override_duration_ms=300000` tại L1.

---

## 4. MSG_RENEW_OVERRIDE (C1 -> Lx)

Xử lý bởi `lx_fsm_on_renew_override()`. Điều kiện ACK: phải đang
`SUPERVISORY_CENTRAL_OVERRIDE` **và** `override_substate == OVR_ACTIVE`
(chú ý: `OVR_PENDING_CLEARANCE` KHÔNG đủ điều kiện - vẫn NACK
`UNKNOWN_TARGET`). NACK thứ hai: `extend_duration_ms > 300000` ->
`INVALID_DURATION`. `extend_duration_ms=0` nghĩa là "gia hạn lại đúng
bằng duration gốc" (`override_duration_ms`, không phải 0ms).

### TC-MSG-19: RENEW_OVERRIDE hợp lệ trên override đang ACTIVE - ACK
- **Loại**: Positive
- **Verb**: MSG_RENEW_OVERRIDE
- **Liên quan**: (không có nack)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Thực hiện TC-MSG-9 (override ACTIVE, 30000ms, còn hiệu lực).
- **Các bước**: C1: `r` -> `Lx number: 1` -> `extend_duration_ms: 60000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_duration_ms` và `override_remaining_ms` tại L1 = 60000 (đếm lại từ đầu, không cộng dồn với thời gian đã trôi qua).

### TC-MSG-20: RENEW_OVERRIDE khi không có override nào - NACK UNKNOWN_TARGET
- **Loại**: Negative
- **Verb**: MSG_RENEW_OVERRIDE
- **Liên quan**: NACK_REASON_UNKNOWN_TARGET
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 ở NORMAL_OPERATION, chưa từng có override nào được gửi.
- **Các bước**: C1: `r` -> `1` -> `extend_duration_ms: 5000` (operator sẽ in cảnh báo "Central has no override recorded in flight" nhưng vẫn gửi đi - đúng theo thiết kế BR-7).
- **Kết quả mong đợi**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET`.

### TC-MSG-21: RENEW_OVERRIDE trên override đang OVR_PENDING_CLEARANCE (chưa active) - NACK UNKNOWN_TARGET (edge case)
- **Loại**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Liên quan**: NACK_REASON_UNKNOWN_TARGET; điều kiện chính xác `override_substate != OVR_ACTIVE` (bao gồm cả PENDING_CLEARANCE) trong `lx_fsm_on_renew_override()`
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Tái hiện TC-MSG-10 (override đang `OVR_PENDING_CLEARANCE`, chờ ped clearance kết thúc).
- **Các bước**: Trong lúc override còn ở PENDING_CLEARANCE (chưa chuyển ACTIVE), gửi ngay: C1: `r` -> `1` -> `extend_duration_ms: 0`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET` (mặc dù về logic nghiệp vụ override "đang tồn tại", code chỉ coi renew hợp lệ khi đã thật sự ACTIVE). Override PENDING_CLEARANCE ban đầu không bị ảnh hưởng bởi lần renew bị từ chối này.

### TC-MSG-22: RENEW_OVERRIDE extend_duration_ms=300001 - NACK INVALID_DURATION
- **Loại**: Negative
- **Verb**: MSG_RENEW_OVERRIDE
- **Liên quan**: NACK_REASON_INVALID_DURATION
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Override đang ACTIVE (TC-MSG-9).
- **Các bước**: C1: `r` -> `1` -> `extend_duration_ms: 300001`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=INVALID_DURATION`. Hạn override hiện tại (expiry cũ) giữ nguyên, không đổi (comment code: "Retain the current expiry unchanged on rejection").

### TC-MSG-23: extend_duration_ms=0 - ACK, gia hạn đúng bằng duration GỐC (edge case)
- **Loại**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Liên quan**: ngữ nghĩa đặc biệt của giá trị 0 (`renew_override_payload_t.extend_duration_ms`)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Tạo override ban đầu với `duration_ms=20000` (TC-MSG-9 kiểu, dùng 20000 thay vì 30000). Đợi vài giây để `override_remaining_ms` giảm xuống dưới 20000 (ví dụ còn ~15000ms).
- **Các bước**: C1: `r` -> `1` -> `extend_duration_ms: 0`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_remaining_ms` được đặt LẠI = 20000 (giá trị GỐC `override_duration_ms`, không phải cộng thêm 0 vào phần còn lại ~15000, và không phải bị đặt = 0).

### TC-MSG-24: extend_duration_ms=300000 - ACK (biên trên hợp lệ)
- **Loại**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Liên quan**: biên trên `LX_OVERRIDE_DURATION_CAP_MS`
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Override đang ACTIVE.
- **Các bước**: C1: `r` -> `1` -> `extend_duration_ms: 300000`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_duration_ms=override_remaining_ms=300000`.

### TC-MSG-25: extend_duration_ms=1 - ACK (biên dưới, giá trị gia hạn thật nhỏ nhất >0)
- **Loại**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Liên quan**: phân biệt với case "0 = giữ nguyên" (TC-MSG-23) - đây là gia hạn tường minh 1ms
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Override đang ACTIVE.
- **Các bước**: C1: `r` -> `1` -> `extend_duration_ms: 1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_duration_ms=override_remaining_ms=1` (khác 0 - đây là 1ms thật, không phải "giữ nguyên gốc"), override kết thúc ngay ở tick tiếp theo.

---

## 5. MSG_CANCEL_OVERRIDE (C1 -> Lx)

Xử lý bởi `lx_fsm_on_cancel_override()`. Không có payload trên dây. ACK
nếu `override_substate` là `OVR_ACTIVE` **hoặc** `OVR_PENDING_CLEARANCE`
(cả hai đều bị hủy qua `lx_fsm_terminate_override_locked()`); NACK
`UNKNOWN_TARGET` nếu `OVR_NONE`.

### TC-MSG-26: CANCEL_OVERRIDE trên override đang ACTIVE - ACK
- **Loại**: Positive
- **Verb**: MSG_CANCEL_OVERRIDE
- **Liên quan**: (không có nack)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Override ACTIVE (TC-MSG-9, duration dài, ví dụ 60000ms để có thời gian thao tác).
- **Các bước**: C1: `c` -> `Lx number: 1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> ACK`. L1: `override_substate=OVR_NONE`, `supervisory` trở về `SUPERVISORY_NORMAL_OPERATION` ngay lập tức (an toàn - qua "safe clearance" placeholder).

### TC-MSG-27: CANCEL_OVERRIDE trên override đang OVR_PENDING_CLEARANCE (chưa active) - ACK (edge case)
- **Loại**: Edge case
- **Verb**: MSG_CANCEL_OVERRIDE
- **Liên quan**: nhánh `override_substate == OVR_PENDING_CLEARANCE` cũng được chấp nhận hủy (khác với RENEW_OVERRIDE ở TC-MSG-21, vốn từ chối case này)
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: Tái hiện TC-MSG-10 (override PENDING_CLEARANCE, đang chờ ped clearance).
- **Các bước**: Trong lúc còn PENDING_CLEARANCE, gửi ngay C1: `c` -> `1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> ACK`. Override bị hủy hoàn toàn dù chưa từng active; khi ped clearance kết thúc, KHÔNG có override nào được kích hoạt (đối chiếu với TC-MSG-10 nếu không cancel).

### TC-MSG-28: CANCEL_OVERRIDE khi không có override - NACK UNKNOWN_TARGET
- **Loại**: Negative
- **Verb**: MSG_CANCEL_OVERRIDE
- **Liên quan**: NACK_REASON_UNKNOWN_TARGET
- **Môi trường**: (B) C1 + L1
- **Chuẩn bị**: L1 ở NORMAL_OPERATION, không có override nào.
- **Các bước**: C1: `c` -> `1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET`.

### TC-MSG-29: Gửi 2 CANCEL_OVERRIDE gần như đồng thời tới cùng 1 Lx (duplicate nhanh / race)
- **Loại**: Edge case
- **Verb**: MSG_CANCEL_OVERRIDE
- **Liên quan**: tính idempotent và tuần tự hóa (serialize) dưới `fsm->lock` khi 2 request cùng target đến gần như cùng lúc
- **Môi trường**: (D) - **bắt buộc**. `c_operator.c` đọc bàn phím tuần tự trên 1 luồng duy nhất (`c_operator_reader_thread`), nên 2 lần gõ `c` luôn cách nhau ít nhất 1 lượt `scanf` - không thể tạo ra 2 message CANCEL_OVERRIDE thực sự đồng thời từ 1 tiến trình C1 duy nhất. Cần 2 tiến trình test_client độc lập, mỗi tiến trình tự mở kết nối riêng và gửi `MsgSend()` gần như cùng một thời điểm (ví dụ đồng bộ qua 1 named semaphore/barrier) tới cùng L1.
- **Chuẩn bị**: Override đang ACTIVE tại L1.
- **Các bước**: 2 tiến trình test_client A và B, mỗi tiến trình gửi `MSG_CANCEL_OVERRIDE{target_id=CTRL_L1}` trong cùng 1 cửa sổ vài mili-giây (dùng busy-wait tới 1 mốc thời gian chung, hoặc bắn liên tiếp không chờ reply nếu test_client hỗ trợ gửi bất đồng bộ).
- **Kết quả mong đợi**: Vì `ipc_server_run()` xử lý tuần tự từng `MsgReceive()` trên 1 channel (không có 2 luồng server chạy song song tại L1), đúng 1 trong 2 request nhận `RESULT_ACK` (request tới trước), request còn lại nhận `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET` (override đã bị hủy bởi request kia) - không có trạng thái không nhất quán, không crash, không double-free/double-terminate override.

---

## 6. MSG_REQUEST_FAULT_CLEAR (C1 -> RLx / Lx)

Xử lý bởi `rlx_fsm_on_fault_clear()` ở RLx. `NACK_REASON_UNKNOWN_TARGET` nếu
`state != RLX_FAULT`; nếu đang `RLX_FAULT`: `RESULT_ACK` nếu
`rlx_gate_poll_open()==1`, ngược lại `NACK_REASON_FAULT_ACTIVE`.

**Cập nhật (test-plan finding đã được sửa)**: verb này ban đầu chỉ tài liệu
hóa là C1->RLx; `lx_fsm_local_fault_clear()` tồn tại ở phía Lx nhưng không
verb/case nào gọi tới nó, nên gửi `MSG_REQUEST_FAULT_CLEAR` tới một Lx từng
trả về `RESULT_ERROR` (xem TC-MSG-33 cũ). Việc này đã được nối dây: `lx_main.c`'s
`on_request()` nay có case `MSG_REQUEST_FAULT_CLEAR` gọi
`lx_fsm_on_request_fault_clear()` (`lx_fsm.h`/`lx_fsm.c`), và `c_operator.c`'s
`handle_request_fault_clear()` hỏi `node type` (0=Lx, 1=RLx) trước khi hỏi số
hiệu, nên phím `f` tại C1 nay nhắm được cả hai loại node. TC-MSG-33 dưới đây
phản ánh hành vi hiện tại thay vì `RESULT_ERROR` cũ.

**Phát hiện quan trọng**: đọc kỹ `enter_fault()` (`rlx_fsm.c`) cho thấy
mọi đường vào `RLX_FAULT` đều gọi `rlx_gate_command_close()` (không
bao giờ gọi `rlx_gate_command_open()`), và không có bất kỳ code nào
khác gọi lệnh mở cổng trong khi `state == RLX_FAULT`. Do đó
`rlx_gate_poll_open()` **không thể** trở thành 1 khi đang `RLX_FAULT`
với code hiện tại -> nhánh `RESULT_ACK` của verb này dường như
**không thể tái hiện được** bằng bất kỳ chuỗi thao tác nào trên bản
build hiện tại (xem TC-MSG-32).

### TC-MSG-30: REQUEST_FAULT_CLEAR khi RLx không ở trạng thái FAULT - NACK UNKNOWN_TARGET
- **Loại**: Negative
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Liên quan**: NACK_REASON_UNKNOWN_TARGET
- **Môi trường**: (B) C1 + RL1
- **Chuẩn bị**: RL1 ở `RLX_OPEN` (mặc định khi khởi động, chưa có tàu, chưa fault).
- **Các bước**: C1: `f` -> `RLx number (1-3): 1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> NACK reason=UNKNOWN_TARGET` (target_id in ra là giá trị số của `CTRL_RL1`, =7 theo `controller_id_t`).

### TC-MSG-31: REQUEST_FAULT_CLEAR khi RLx đang FAULT nhưng cổng chưa xác nhận mở - NACK FAULT_ACTIVE
- **Loại**: Negative
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Liên quan**: NACK_REASON_FAULT_ACTIVE
- **Môi trường**: (B)/(C) C1 + RL1
- **Chuẩn bị**: Tại RL1's sensor console: bấm `x` (arm demo gate-fail) NGAY TRƯỚC KHI bấm `0` (TRAIN_APPROACHING direction 0). Đợi đủ `RLX_WARNING_TO_CLOSING_MS (5s)` để RL1 vào CLOSING, rồi đợi thêm tới `RLX_CLOSING_DEADLINE_MS (15s kể từ lúc vào CLOSING)` để `check_closing_or_reclosing_complete()` phát hiện gate không xác nhận đóng và gọi `enter_fault(FAULT_GATE_CONFIRM_MISSING)` (tổng thời gian chờ ~20s). Xác nhận qua log RLx: gate "FAILED TO CONFIRM" rồi state chuyển FAULT.
- **Các bước**: C1: `f` -> `1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> NACK reason=FAULT_ACTIVE` (vì `enter_fault()` đã tự phát lệnh đóng cổng LẦN NỮA - lần này không bị armed fail nữa nên 3s sau sẽ xác nhận ĐÓNG, không phải MỞ - `gates_confirmed_open()` vẫn = 0).

### TC-MSG-32: REQUEST_FAULT_CLEAR -> ACK (trường hợp tích cực) - HIỆN KHÔNG THỂ TÁI HIỆN, cần sửa code hoặc công cụ bổ sung
- **Loại**: Positive (BLOCKED - phát hiện khoảng trống trong thiết kế/hiện thực)
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Liên quan**: đường "ACK" của `rlx_fsm_on_fault_clear()` (điều kiện: `state==RLX_FAULT` và `rlx_gate_poll_open()==1`)
- **Môi trường**: (D) - cần công cụ HOẶC một bản vá tạm thời, không chỉ là test_client thông thường (xem giải thích).
- **Chuẩn bị/Giải thích**: Theo code hiện tại, **không có bất kỳ đường thực thi nào** đặt `g_confirmed_open=1` trong khi `fsm->state == RLX_FAULT`: `enter_fault()` (được gọi từ mọi nơi dẫn tới FAULT - deadline miss khi CLOSING/RECLOSING/OPENING, hay watchdog trip) luôn gọi `rlx_gate_command_close()`, không bao giờ `rlx_gate_command_open()`; và `rlx_fsm_on_tick()`'s case `RLX_FAULT` là no-op (không lệnh gì thêm). Vì test_client vẫn gọi cùng 1 `rlx_fsm_on_fault_clear()` với cùng state nội bộ đó, việc gửi request qua đường dây không giúp ích - đây là giới hạn ở tầng FSM/gate simulator, không phải ở tầng giao thức IPC.
- **Đề xuất khắc phục để test case này khả thi**: bổ sung 1 phím DEMO-ONLY vào `rlx_sensor.c` (cùng khuôn mẫu với `x`/`f` đã có) mô phỏng "kỹ thuật viên đã sửa xong và xác nhận cổng mở tay", gọi thẳng `rlx_gate_command_open()` (hoặc set thẳng cờ nội bộ) trong khi đang FAULT, rồi đợi `RLX_GATE_MOTION_MS=3000ms` để `rlx_gate_poll_open()` trả 1.
- **Các bước (sau khi có bản vá)**: Vào FAULT như TC-MSG-31 -> bấm phím DEMO-ONLY mới để mở cổng -> đợi 3s -> C1: `f` -> `1`.
- **Kết quả mong đợi (sau khi có bản vá)**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> ACK`; RL1 trở lại `RLX_OPEN`, `faults=FAULT_NONE`, các cửa sổ occupancy được xóa.

### TC-MSG-33: REQUEST_FAULT_CLEAR gửi tới một Lx đang FAULT_SAFE - ACK (đã sửa, không còn RESULT_ERROR)
- **Loại**: Positive (trước đây là Edge case/Negative "RESULT_ERROR" - hành vi đó đã lỗi thời, xem mục 6's "Cập nhật")
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Liên quan**: `lx_fsm_on_request_fault_clear()` (`lx_fsm.c`) - nay có case riêng trong `lx_main.c`'s `on_request()`, không còn rơi vào `default:`. `c_operator.c`'s phím `f` hỏi node type (0=Lx, 1=RLx) qua `handle_request_fault_clear()`, nên có thể nhắm L1 trực tiếp từ console mà không cần test_client.
- **Môi trường**: (B) C1 + L1 là đủ (không còn bắt buộc (D)/test_client cho case cơ bản này).
- **Chuẩn bị**: Đưa L1 vào `SUPERVISORY_FAULT_SAFE` (ví dụ qua watchdog trip, xem TC-SC03A-6 Phần 1 của `02-state-machine-transition.md`), và đảm bảo `last_crossing_state==CROSSING_OPEN` (không có RLx kề nào đang pre-empt) để kỳ vọng resume đúng `NORMAL_OPERATION`.
- **Các bước**: Tại C1: `f` -> node type `0` (Lx) -> Lx number `1`.
- **Kết quả mong đợi**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`. `fsm->faults` về `FAULT_NONE`, SUPERVISORY L1 rời `FAULT_SAFE` về `NORMAL_OPERATION` (`3`). Hàm này không có nhánh NACK (unconditional/idempotent, khác `rlx_fsm_on_fault_clear()` - không có trạng thái vật lý nào phải re-verify ở Lx).

### TC-MSG-33b: REQUEST_FAULT_CLEAR tới một Lx đang FAULT_SAFE trong lúc crossing kề bên vẫn đóng - resume RAILWAY_PREEMPTION, không phải NORMAL_OPERATION (re-audit fix, an toàn)
- **Loại**: Positive (safety-relevant regression case)
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Liên quan**: `last_crossing_state` (`lx_fsm.h`) - re-audit fix: một fault-clear xảy ra trong lúc crossing kề bên vẫn chưa `OPEN` không được phép âm thầm quên mất việc suppress đang có, kẻo cho phép green hướng về một crossing vẫn còn đóng.
- **Môi trường**: (B) C1 + L1 + RL1.
- **Chuẩn bị**: Đưa L1 vào `RAILWAY_PREEMPTION` thật (RL1 ở WARNING/CLOSED, gửi `CROSSING_STATUS` khác `CROSSING_OPEN` tới L1), sau đó trip watchdog để L1 vào `FAULT_SAFE` trong khi vẫn đang pre-empt (SUPERVISORY chuyển thẳng `1 -> 0`, `fsm->last_crossing_state` vẫn giữ giá trị non-OPEN gần nhất vì `lx_fsm_on_crossing_status()` cập nhật trường này vô điều kiện, kể cả khi đang FAULT_SAFE).
- **Các bước**: Tại C1: `f` -> `0` (Lx) -> `1`, **trước khi** RL1 kịp báo `OPEN` trở lại.
- **Kết quả mong đợi**: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`, nhưng SUPERVISORY L1 sau đó phải là `RAILWAY_PREEMPTION` (`1`), **không phải** `NORMAL_OPERATION` (`3`) - CONNECTOR_GREEN (hướng crossing) vẫn bị suppress cho tới khi L1 thực sự nhận `CROSSING_STATUS(OPEN)` từ RL1. Đây là hành vi ĐÚNG theo thiết kế mới (trước bản vá, code cũ luôn resume `NORMAL_OPERATION` vô điều kiện, có thể cho phép green hướng về crossing đang đóng).

---

## 7. MSG_STATUS (Lx/RLx -> C1)

`c_main.c`'s `on_request()` xử lý `MSG_STATUS` giống hệt `MSG_HEARTBEAT`
(luôn `RESULT_ACK`, gọi `c_server_record_status()`, không có nhánh NACK
nào). **Phát hiện quan trọng**: rà toàn bộ `lx_comm.c` và `rlx_comm.c`
xác nhận **không có hàm nào gửi `MSG_STATUS`** - cả hai file chỉ có
`*_send_heartbeat()`, và (RLx) `*_send_fault_report()`,
`*_broadcast_crossing_status_if_changed()`. Verb này được định nghĩa,
được C1 sẵn sàng xử lý, nhưng **chưa có sender nào trong toàn bộ
codebase hiện tại**. Do đó mọi test case cho verb này bắt buộc dùng
test_client.

### TC-MSG-34: MSG_STATUS hợp lệ tới C1 - ACK
- **Loại**: Positive
- **Verb**: MSG_STATUS
- **Liên quan**: (không có nack - và không có sender thật trong code, xem ghi chú trên)
- **Môi trường**: (D) - bắt buộc, vì không node nào trong code hiện tại tạo ra verb này.
- **Chuẩn bị**: `c_main` đang chạy.
- **Các bước**: test_client gửi `ipc_request_t{verb=MSG_STATUS, sender_id=CTRL_L2, target_id=CTRL_C1, payload.status={role=ROLE_INTERSECTION, mode=MODE_PEAK_FIXED, signal_phase=PHASE_ARTERIAL_GREEN, supervisory_state=SUPERVISORY_NORMAL_OPERATION, ...}}` tới C1.
- **Kết quả mong đợi**: reply `result=RESULT_ACK`. `c_mode_eng_t.controllers[idx]` (idx tương ứng CTRL_L2) được cập nhật đúng các trường `last_reported_*`; `missed_heartbeat_ticks` và `marked_unavailable` được reset về 0 (side-effect giống hệt nhận HEARTBEAT/CROSSING_STATUS - xem `c_server_record_status()`).

### TC-MSG-35: MSG_STATUS với role không khớp sender_id thật - vẫn ACK (edge case robustness)
- **Loại**: Edge case
- **Verb**: MSG_STATUS
- **Liên quan**: không có validation chéo giữa `sender_id` và `payload.status.role` trong `c_server_record_status()`
- **Môi trường**: (D)
- **Chuẩn bị**: Giống TC-MSG-34.
- **Các bước**: test_client gửi `MSG_STATUS` với `sender_id=CTRL_L2` nhưng `payload.status.role=ROLE_RAILWAY` (cố tình sai).
- **Kết quả mong đợi**: reply vẫn `result=RESULT_ACK` (C1 không kiểm tra field `role` khớp với sender_id nào - chỉ dùng `sender_id` để tra `c_mode_eng_controller_index()`). Đây là kết quả ĐÚNG với code hiện tại (không phải bug per-se), nhưng cần ghi nhận: HMI (`c_hmi.c`) có thể hiển thị sai nếu 1 node tự nhận nhầm role - khuyến nghị bổ sung validation nếu muốn phòng thủ chống node bị lỗi/giả mạo.

---

## 8. MSG_FAULT_REPORT (RLx -> C1)

Gửi bởi `rlx_comm_send_fault_report()`, kích hoạt đúng lúc
`rlx_fsm_take_fault_report_pending()` trả về 1 (tức thời điểm
`enter_fault()` chạy). C1's `on_request()` luôn `RESULT_ACK`, không có
NACK. `c_server_record_fault_report()` hiện là **no-op hoàn toàn**
(chỉ `c_logger_log()` tại `c_main.c` mới thực sự in ra thông tin).

### TC-MSG-36: MSG_FAULT_REPORT khi RLx thật sự vào FAULT - ACK
- **Loại**: Positive
- **Verb**: MSG_FAULT_REPORT
- **Liên quan**: (không có nack)
- **Môi trường**: (B)/(C) C1 + RL1
- **Chuẩn bị**: Giống chuẩn bị TC-MSG-31 (bấm `x` rồi `0` tại RL1, đợi ~20s vào FAULT_GATE_CONFIRM_MISSING).
- **Các bước**: Không cần thao tác gì thêm tại C1 - RLx tự động gửi `MSG_FAULT_REPORT` ngay khi vào FAULT (trong cùng tick armed `IPC_PULSE_RAILWAY_WARNING`, xem `rlx_main.c`'s `on_pulse()`).
- **Kết quả mong đợi**: `central_log.txt` có dòng do `c_main.c` in trực tiếp: `FAULT_REPORT from 7: fault_code=0x00000001 severity=1 detail="RLx fault - see fault_code bitmask"` (fault_code = `FAULT_GATE_CONFIRM_MISSING` = bit 0 = 0x1). Không có dòng NACK nào (verb này luôn ACK).

### TC-MSG-37: MSG_FAULT_REPORT với detail[64] không có null-terminator - kiểm tra an toàn bộ nhớ (edge case robustness)
- **Loại**: Edge case
- **Verb**: MSG_FAULT_REPORT
- **Liên quan**: an toàn xử lý chuỗi trong `c_logger_log()`/`printf("%s", ...)` tại `c_main.c` khi nhận payload cố tình sai định dạng
- **Môi trường**: (D) - `rlx_comm_send_fault_report()` luôn tự `strncpy` + ép `\0` byte cuối, không có đường nào trong code thật tạo ra chuỗi thiếu null-terminator; phải test_client cố tình vi phạm.
- **Chuẩn bị**: `c_main` đang chạy.
- **Các bước**: test_client gửi `ipc_request_t{verb=MSG_FAULT_REPORT, sender_id=CTRL_RL2, target_id=CTRL_C1, payload.fault_report={fault_code=0xFF, severity=9, detail=<64 byte đều là ký tự 'A', KHÔNG có '\0' ở cuối>}}`.
- **Kết quả mong đợi**: Tiến trình `c_main` KHÔNG crash/segfault (an toàn bộ nhớ), reply `result=RESULT_ACK` vẫn được trả về bình thường. Ghi nhận riêng: vì `req->payload.fault_report.detail` nằm giữa 1 struct lớn hơn (`ipc_request_t`), `printf("%s", ...)` khi thiếu `\0` có thể đọc tràn sang field kế tiếp trong bộ nhớ tiến trình cho tới khi gặp byte 0 ngẫu nhiên - log dòng in ra có thể chứa rác nhưng không được phép crash tiến trình. Nếu quan sát thấy crash, đây là 1 lỗ hổng an toàn bộ nhớ cần báo cáo (Auditor/Verifier agent nên đánh dấu FAIL).

---

## 9. MSG_CROSSING_STATUS (RLx -> Lx kề cận; RLx -> C1)

Gửi bởi `rlx_comm_broadcast_crossing_status_if_changed()`, CHỈ khi
`crossing_state_t` suy ra từ state nội bộ thực sự đổi so với lần gửi
trước (dedup qua biến static `last_broadcast_state`). Bên nhận (cả
`lx_fsm_on_crossing_status()` và C1's `on_request()`) luôn `RESULT_ACK`
- RC-02: "Lx chỉ quan sát, không bao giờ từ chối".

### TC-MSG-38: CROSSING_STATUS(WARNING) tới Lx kề cận - ACK, Lx vào RAILWAY_PREEMPTION
- **Loại**: Positive
- **Verb**: MSG_CROSSING_STATUS
- **Liên quan**: (không có nack)
- **Môi trường**: (B)/(C) L1 + RL1 (không bắt buộc có C1 để test riêng cạnh RLx<->Lx, nhưng nên có đủ để log rõ ràng)
- **Chuẩn bị**: L1, RL1 khởi động, L1 ở NORMAL_OPERATION.
- **Các bước**: Tại RL1: bấm `0` (TRAIN_APPROACHING) -> RL1 chuyển OPEN->WARNING, tự động gửi `MSG_CROSSING_STATUS{state=CROSSING_WARNING}` tới L1, L2, C1.
- **Kết quả mong đợi**: L1 trả `result=RESULT_ACK`; `L1.supervisory` chuyển thành `SUPERVISORY_RAILWAY_PREEMPTION` (nếu trước đó có override CENTRAL_OVERRIDE đang chạy, override đó bị hủy an toàn trước - xem `lx_fsm_on_crossing_status()`).

### TC-MSG-39: CROSSING_STATUS tới C1 - ACK, C1 cập nhật last_reported_crossing_state
- **Loại**: Positive
- **Verb**: MSG_CROSSING_STATUS
- **Liên quan**: (không có nack)
- **Môi trường**: (B)/(C) C1 + RL1
- **Chuẩn bị**: Giống TC-MSG-38.
- **Các bước**: Quan sát C1 nhận `MSG_CROSSING_STATUS` cùng lúc RL1 gửi tới L1/L2.
- **Kết quả mong đợi**: C1's `on_request()` case `MSG_CROSSING_STATUS` trả `result=RESULT_ACK`; `c_server_record_crossing_status()` cập nhật `last_reported_crossing_state=CROSSING_WARNING` cho index của RL1, đồng thời reset `missed_heartbeat_ticks=0`/`marked_unavailable=0`.

### TC-MSG-40: Không gửi lặp CROSSING_STATUS khi trạng thái không đổi (dedup - edge case)
- **Loại**: Edge case
- **Verb**: MSG_CROSSING_STATUS
- **Liên quan**: cơ chế `last_broadcast_state` trong `rlx_comm_broadcast_crossing_status_if_changed()`
- **Môi trường**: (B)/(C) C1 + RL1 + L1
- **Chuẩn bị**: RL1 đang ở `RLX_WARNING` (đã gửi CROSSING_WARNING 1 lần, như TC-MSG-38/39).
- **Các bước**: Không thao tác gì thêm - để RL1 tự tick (`IPC_PULSE_RAILWAY_WARNING`, mỗi 1s) trong vài giây trong khi state nội bộ vẫn map ra `CROSSING_WARNING` (`RLX_WARNING`/`RLX_CLOSING`/`RLX_RECLOSING` đều map cùng 1 giá trị wire - xem `map_to_crossing_state()`). Ví dụ: đợi RL1 tự chuyển từ `RLX_WARNING` sang `RLX_CLOSING` sau 5s (`RLX_WARNING_TO_CLOSING_MS`) - đây là đổi state NỘI BỘ nhưng KHÔNG đổi state TRÊN DÂY.
- **Kết quả mong đợi**: Log tại L1/C1 chỉ hiện đúng 1 dòng nhận `MSG_CROSSING_STATUS(WARNING)` (từ lúc OPEN->WARNING) - **không** có thêm dòng nào khi RL1 chuyển nội bộ WARNING->CLOSING, dù RL1 tick mỗi giây và gọi `rlx_comm_broadcast_crossing_status_if_changed()` mỗi lần. Chỉ khi cổng xác nhận đóng thật (chuyển sang `RLX_CLOSED`, map ra `CROSSING_CLOSED` - khác `CROSSING_WARNING`) mới có dòng CROSSING_STATUS thứ 2 xuất hiện.

### TC-MSG-41: CROSSING_STATUS với state ngoài phạm vi enum hợp lệ - vẫn ACK, được hiểu là "không OPEN" (edge case robustness)
- **Loại**: Edge case
- **Verb**: MSG_CROSSING_STATUS
- **Liên quan**: không có validation range cho field `state` trong `lx_fsm_on_crossing_status()` (chỉ so sánh `!= CROSSING_OPEN`)
- **Môi trường**: (D) - không có đường nào trong `rlx_comm.c` tạo ra giá trị `state` ngoài 4 giá trị hợp lệ của `crossing_state_t` (0-3); phải test_client tự chế payload.
- **Chuẩn bị**: L1 ở NORMAL_OPERATION.
- **Các bước**: test_client gửi `ipc_request_t{verb=MSG_CROSSING_STATUS, sender_id=CTRL_RL1, target_id=CTRL_L1, payload.crossing_status={state=999}}`.
- **Kết quả mong đợi**: reply `result=RESULT_ACK` (RC-02: Lx không bao giờ từ chối verb này bất kể nội dung). Vì `999 != CROSSING_OPEN(0)`, L1 vẫn xử lý y hệt như nhận được "không OPEN" -> chuyển `SUPERVISORY_RAILWAY_PREEMPTION`. Đây là hành vi ĐÚNG theo code hiện tại, nhưng ghi nhận: không có cơ chế nào phát hiện giá trị enum rác từ 1 node bị lỗi.

---

## 10. MSG_HEARTBEAT (Lx/RLx -> C1)

Gửi tự động mỗi 1000ms bởi cả `lx_comm_send_heartbeat()` và
`rlx_comm_send_heartbeat()` (armed timer `IPC_PULSE_HEARTBEAT_TICK`,
không cần thao tác bàn phím nào để xảy ra). C1's `on_request()` luôn
`RESULT_ACK`. PA-07: 3 tick liên tiếp (1 tick/giây tại C1's
`IPC_PULSE_HEARTBEAT_TICK`, xem `c_watchdog_mon.c`) không nhận được
proof-of-life nào (STATUS/HEARTBEAT/CROSSING_STATUS) từ 1 controller ->
`marked_unavailable=1`.

### TC-MSG-42: HEARTBEAT định kỳ 1Hz từ Lx - ACK liên tục
- **Loại**: Positive
- **Verb**: MSG_HEARTBEAT
- **Liên quan**: (không có nack)
- **Môi trường**: (A)/(B) chỉ cần C1 + 1 Lx (ví dụ L1) chạy song song, không cần thao tác gì
- **Chuẩn bị**: Khởi động `c_main`, `lx_main 1`.
- **Các bước**: Không thao tác gì - chỉ chờ và quan sát trong >= 5 giây.
- **Kết quả mong đợi**: `c_main`'s `on_request()` case `MSG_HEARTBEAT` trả `RESULT_ACK` cho mỗi lần nhận (không có log riêng cho ACK ở đường HEARTBEAT trong `c_main.c` - xác nhận gián tiếp qua `c_mode_eng.controllers[idx].missed_heartbeat_ticks` luôn ở 0 và HMI (`c_hmi_render()`, chạy 1Hz) hiển thị L1 là "còn sống"/không bị đánh dấu unavailable).

### TC-MSG-43: Mất 3 nhịp tim liên tiếp - Lx bị đánh dấu unavailable (PA-07, edge case)
- **Loại**: Edge case
- **Verb**: MSG_HEARTBEAT
- **Liên quan**: PA-07 (`c_watchdog_mon_tick()`, ngưỡng 3 tick trong `c_mode_eng.h`'s comment)
- **Môi trường**: (B)/(C) C1 + L1
- **Chuẩn bị**: `c_main`, `lx_main 1` đang chạy ổn định (đã thấy HEARTBEAT đều đặn).
- **Các bước**: Dừng đột ngột tiến trình `lx_main 1` (Ctrl+C hoặc `slay lx_main`/`kill`) để mô phỏng mất kết nối hoàn toàn. Chờ >= 3 giây (3 tick của `IPC_PULSE_HEARTBEAT_TICK` tại C1).
- **Kết quả mong đợi**: Sau đúng 3 tick không nhận được HEARTBEAT/STATUS/CROSSING_STATUS nào từ CTRL_L1, `c_mode_eng.controllers[idx].marked_unavailable` chuyển từ 0 -> 1 (edge-triggered, chỉ log 1 lần "went stale"); `c_hmi_render()` (chạy mỗi giây) hiển thị L1 là không khả dụng/mất kết nối kể từ lần render kế tiếp.

### TC-MSG-44: HEARTBEAT từ RLx - role=ROLE_RAILWAY, mode/signal_phase=0 không bị hiểu nhầm
- **Loại**: Positive (kiểm tra đúng ngữ nghĩa field dùng chung giữa Lx/RLx)
- **Verb**: MSG_HEARTBEAT
- **Liên quan**: (không có nack) - `status_report_payload_t` dùng chung cho cả 2 role, `mode`/`signal_phase`/`supervisory_state` "meaningful for ROLE_INTERSECTION" only (comment `ipc_msg.h`)
- **Môi trường**: (B)/(C) C1 + RL1
- **Chuẩn bị**: `c_main`, `rlx_main 1` chạy, RL1 ở `RLX_OPEN`.
- **Các bước**: Không thao tác gì - chờ HEARTBEAT tự động (1Hz).
- **Kết quả mong đợi**: reply `RESULT_ACK`. Payload nhận được có `role=ROLE_RAILWAY`, `crossing_state=CROSSING_OPEN(0)`, còn `mode=0`, `signal_phase=0`, `supervisory_state=0` (giá trị mặc định memset, KHÔNG có ý nghĩa cho RLx - `rlx_fsm_fill_status()` chỉ điền `role`/`crossing_state`/`faults`). Xác nhận `c_mode_eng.controllers[idx].last_reported_mode` cho RL1 = 0 KHÔNG được HMI diễn giải nhầm thành `MODE_PEAK_FIXED` có ý nghĩa thật (chỉ nên hiển thị field có ý nghĩa theo role).

### TC-MSG-45: HEARTBEAT với role field ngoài phạm vi enum hợp lệ - vẫn ACK (negative/robustness)
- **Loại**: Negative
- **Verb**: MSG_HEARTBEAT
- **Liên quan**: không có validation nào cho `payload.heartbeat.summary.role`/các field enum khác tại `c_main.c`
- **Môi trường**: (D) - không đường nào trong `lx_comm.c`/`rlx_comm.c` tạo ra role ngoài `{ROLE_CENTRAL, ROLE_INTERSECTION, ROLE_RAILWAY}`; phải test_client tự chế.
- **Chuẩn bị**: `c_main` đang chạy.
- **Các bước**: test_client gửi `ipc_request_t{verb=MSG_HEARTBEAT, sender_id=CTRL_L4, target_id=CTRL_C1, payload.heartbeat.summary={role=99, ...}}`.
- **Kết quả mong đợi**: reply vẫn `result=RESULT_ACK` (C1 không bao giờ NACK bất kỳ nội dung nào của HEARTBEAT/STATUS/CROSSING_STATUS/FAULT_REPORT - 4 verb "báo cáo" này luôn ACK vô điều kiện, chỉ khác nhau ở việc có ghi nhận vào `c_mode_eng_t` hay không). Đây là baseline hành vi cần biết: đội QA không nên kỳ vọng bất kỳ NACK nào xuất hiện từ phía C1 cho 4 verb báo cáo, dù payload có sai định dạng thế nào - mọi kiểm tra tính hợp lệ (nếu muốn có) phải được bổ sung thêm vào `c_server.c`, hiện chưa tồn tại.

---

## Phụ lục: Các phát hiện đáng chú ý trong quá trình đọc code (không phải test case, nhưng ảnh hưởng trực tiếp tới khả năng test)

1. **`NACK_REASON_PEDESTRIAN_ACTIVE` là dead code.** Được khai báo trong
   `sys_types.h` với comment "pedestrian clearance active and cannot be
   safely deferred", và có tên hiển thị trong cả `c_comm.c` và
   `c_operator.c`'s `nack_reason_name()`. Nhưng grep toàn bộ `app/`
   cho thấy **không có bất kỳ `lx_fsm_on_*()` nào gán giá trị này** -
   trường hợp ped-clearance đang chạy khi có `REQUEST_OVERRIDE` được xử
   lý bằng `RESULT_ACK_PENDING` (xem TC-MSG-10), không phải NACK. Đây
   là 1 trong 2 khả năng: (a) tài liệu comment ở `sys_types.h` mô tả một
   thiết kế cũ hơn không còn khớp code, hoặc (b) giá trị này dự trù cho
   1 tình huống khác chưa được hiện thực (ví dụ 1 verb khác cũng có thể
   xung đột với ped clearance nhưng không có cơ chế "chờ" như
   override). Đề xuất: Compliance Agent nên đối chiếu lại với tài liệu
   gốc (SD-07/UC-08) để xác nhận đây là API drift cần sửa comment, hay
   1 nhánh code còn thiếu.

2. **`lx_sensor.c` thiếu phím demo ép lỗi thủ công**, không đối xứng với
   `rlx_sensor.c` (đã có `x` để arm gate-fail và `f` để tự fault-clear
   local). Hệ quả: **không test case nào liên quan
   `NACK_REASON_FAULT_ACTIVE` trên Lx** (SET_TIMING_PROFILE, SET_MODE,
   REQUEST_OVERRIDE) có thể chạy bằng thao tác bàn phím thông thường -
   phải chờ PA-10 watchdog trip thật (treo luồng server >= 2s thật sự)
   hoặc bổ sung code. Khuyến nghị Core-Engineer bổ sung 1 phím DEMO-ONLY
   gọi `lx_fsm_report_watchdog_trip(&fsm)` vào `lx_sensor.c`, đúng mẫu
   đã có ở RLx.

3. **`MSG_STATUS` được định nghĩa và C1 sẵn sàng xử lý, nhưng không
   sender nào trong toàn bộ codebase (`lx_comm.c`, `rlx_comm.c`) từng
   gửi verb này.** Toàn bộ báo cáo trạng thái thực tế đi qua
   `MSG_HEARTBEAT` (dùng chung `status_report_payload_t` bên trong
   `heartbeat_payload_t`). Cần làm rõ với Compliance Agent: đây là scope
   creep còn sót (`MSG_STATUS` dự trù cho 1 luồng riêng UC-09 chưa được
   nối dây) hay handler tại C1 là dự phòng không cần thiết.

4. **`RESULT_ACK` của `MSG_REQUEST_FAULT_CLEAR` dường như không thể đạt
   được với code hiện tại** (xem giải thích chi tiết ở TC-MSG-32) - mọi
   đường vào `RLX_FAULT` đều tự động lệnh đóng cổng lại
   (`rlx_gate_command_close()`), không có đường nào mở cổng trong khi
   đang FAULT. Đây là phát hiện quan trọng nhất của tài liệu này: nếu
   đúng như phân tích, RC-09/RC-10's "operator request fault clearance
   after repair" flow **chưa bao giờ có thể ACK** trên bản build hiện
   tại, kể cả trên máy QNX thật với thao tác đúng quy trình - cần Core-
   Engineer xác nhận và vá trước khi coi Phase liên quan tới RC-09 là
   "PASS" ở bước 6 (QA-Test).

5. **`c_server_record_fault_report()` là no-op hoàn toàn** (chỉ
   `c_logger_log()` gọi trực tiếp từ `c_main.c` mới in thông tin fault
   report) - `c_mode_eng_t` không có nơi lưu fault history dài hạn cho
   RLx. Không chặn test giao thức (verb vẫn ACK đúng), nhưng ảnh hưởng
   tới test plan UC-09/HMI (không thuộc phạm vi tài liệu này).

6. **4 verb "báo cáo"** (MSG_STATUS, MSG_HEARTBEAT, MSG_FAULT_REPORT,
   MSG_CROSSING_STATUS khi nhận tại C1, và MSG_CROSSING_STATUS/
   MSG_HEARTBEAT khi nhận tại Lx) **không bao giờ trả về NACK** trong
   toàn bộ codebase hiện tại - `result` chỉ có thể là `RESULT_ACK` (đúng
   route) hoặc `RESULT_ERROR` (verb bị gửi tới node không hỗ trợ nó, ví
   dụ TC-MSG-33). Toàn bộ 8 giá trị `nack_reason_t` trong `sys_types.h`
   chỉ thực sự được dùng cho 5 verb "lệnh" (SET_TIMING_PROFILE,
   SET_MODE, REQUEST_OVERRIDE, RENEW_OVERRIDE, CANCEL_OVERRIDE,
   REQUEST_FAULT_CLEAR) - và trong đó `NACK_REASON_PEDESTRIAN_ACTIVE`
   vẫn không được dùng ở đâu cả (mục 1 phụ lục này).
