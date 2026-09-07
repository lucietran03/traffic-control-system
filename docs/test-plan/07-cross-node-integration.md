# 07. Cross-Node Integration Test Plan (Qnet đa node thật)

## 0. Mục đích & phạm vi

Các file test-plan khác (nếu có) kiểm tra logic **nội bộ một FSM** (một node,
một tiến trình, message giả lập trong cùng process/unit test). File này khác
hẳn: mọi test case ở đây **bắt buộc chạy trên ≥ 2 máy/VM QNX thật, nối với
nhau qua Qnet thật** (không dùng stand-in, không mock `name_open`/`MsgSend`).
Mục tiêu là chứng minh hệ thống thực sự **phân tán** — mỗi node là một tiến
trình độc lập trên một QNX node độc lập, giao tiếp xuyên node qua
`name_attach`/`name_open`/`MsgSend` thật — chứ không chỉ là 10 FSM đúng logic
chạy chung một máy.

Cơ sở kỹ thuật dùng xuyên suốt file này (đã đọc code trước khi viết, không
suy đoán):

- `app/shared/src/qnet_utils.c` / `app/shared/includes/qnet_utils.h`: mọi
  node `name_attach()` một lần với `NAME_FLAG_ATTACH_GLOBAL` dưới
  `traffic/<suffix>`; phía gửi (`ipc_client_thread_main()` → `build_open_path()`)
  tra cứu biến môi trường `TRAFFIC_NODE_MAP` (danh sách
  `"<suffix>=<qnet-nodename>"` cách nhau bởi dấu phẩy) để quyết định có bọc
  `/net/<nodename>/dev/name/global/...` hay không. Suffix vắng mặt trong map
  (kể cả khi biến không được set) = "cùng node với caller" → `name_open()`
  không có tiền tố `/net/`.
- `app/shared/README.md` mục "Cross-node resolution" và
  `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` mục 2.2/2.3 và mục 3 (Case 1/2/3): nguồn
  chính thức cho cú pháp `TRAFFIC_NODE_MAP` và các ví dụ topology dùng lại
  trong file này.
- Gửi đi luôn **non-blocking và graceful khi thất bại**: `ipc_client_post()`
  chỉ enqueue; `ipc_client_thread_main()` là luồng duy nhất gọi
  `name_open()`/`MsgSend()`; nếu `name_open()` trả `-1` (peer không tồn tại /
  chưa chạy / `TRAFFIC_NODE_MAP` sai), `send_ok = 0` được truyền vào
  `on_reply` callback — **không có exception, không crash, không treo tiến
  trình gọi `ipc_client_post()`**. Đây là cơ chế mọi test case "negative" /
  "node chưa lên" trong file này khai thác.
- `app/central/src/c_operator.c`: 6 lệnh operator thật (`m`, `t`, `o`, `r`,
  `c`, `f`) — nguồn duy nhất phát sinh traffic Central → Lx/RLx trong hệ
  thống (không có API nào khác gọi `c_comm_send_*`/`c_comm_broadcast_*`).
- `app/railway/src/rlx_comm.c` hàm
  `rlx_comm_broadcast_crossing_status_if_changed()`: mỗi khi
  `crossing_state_t` đổi, gửi `MSG_CROSSING_STATUS` đúng 3 lần — 2 Lx liền kề
  (bảng adjacency cố định: RL1→{L1,L2}, RL2→{L3,L4}, RL3→{L5,L6}) + C1 — mỗi
  lần gọi `ipc_client_post()` độc lập, không transaction, không rollback nếu
  1 trong 3 lần thất bại.
- `app/intersection/src/lx_comm.c` / `app/railway/src/rlx_comm.c`: heartbeat
  1 giây lên C1 (`MSG_HEARTBEAT`). Phía Central,
  `app/central/src/c_watchdog_mon.c` (`c_watchdog_mon_tick()`, chạy mỗi giây
  qua `IPC_PULSE_HEARTBEAT_TICK`, `ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, ...)`
  ở `c_main.c`) tăng `missed_heartbeat_ticks` mỗi tick; đúng khi bộ đếm chạm
  **3** thì `marked_unavailable = 1` và log
  `"Controller %d marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"`.
  Ngược lại, `c_server.c` (`c_server_record_status()` /
  `c_server_record_crossing_status()`) reset cả
  `missed_heartbeat_ticks = 0` và `marked_unavailable = 0` ngay khi nhận
  **bất kỳ** message hợp lệ (HEARTBEAT, STATUS, hoặc CROSSING_STATUS) từ
  controller đó — **không có log riêng cho việc chuyển lại AVAILABLE**, chỉ
  có thể quan sát qua cột `AVAILABILITY` trong bảng trạng thái
  (`c_hmi.c`, in đúng chuỗi `AVAILABLE` / `UNAVAILABLE`).
- `app/intersection/src/lx_fsm.c` hàm `lx_fsm_apply_offset_locked()`: dùng
  `clock_gettime(CLOCK_REALTIME, ...)` (wall-clock theo epoch, **không phải**
  `CLOCK_MONOTONIC`) để tính lệch pha so với `assigned_offset_ms` nhận từ
  `SET_TIMING_PROFILE`, áp dụng đúng **một lần** tại thời điểm bắt đầu
  `PHASE_ARTERIAL_GREEN` kế tiếp (không cắt ngang pha xanh đang chạy). Đây là
  lý do TC-02/TC-03 (offset xanh liên tuyến) **chỉ có ý nghĩa kiểm chứng
  thật** khi L1/L3/L5 (hoặc L2/L4/L6) chạy trên các VM khác nhau — đồng hồ hệ
  thống độc lập của từng VM mới là biến số thật sự cần test. **Lưu ý quan
  trọng**: code không có bất kỳ cơ chế đồng bộ NTP/time-sync nào; nếu đồng hồ
  các VM lệch nhau, offset áp dụng sẽ lệch theo — đây là giới hạn thiết kế đã
  biết (xem comment trong `lx_fsm.c`), không phải bug, nhưng **phải kiểm tra
  và ghi nhận độ lệch đồng hồ giữa các VM trước khi đánh giá kết quả** của
  các test liên quan tới offset.

## 1. Quy ước dùng trong file này

- **Mã test case**: `TC-XNODE-NN` (NN = 01, 02, ...), không trùng với mã
  `TC-xx` nội bộ FSM ở các file test-plan khác.
- **Loại**: `Positive` (đường đi đúng) / `Negative` (input/điều kiện sai,
  kỳ vọng hệ thống từ chối hoặc fail gracefully) / `Edge case` (biên, hiếm
  gặp nhưng hợp lệ).
- **Liên quan**: assumption ID (`PA-xx`, `TC-xx`, `RC-xx`, `CC-xx`, `UC-xx`,
  `SD-xx`) + file/hàm nguồn cụ thể đã đọc.
- **Môi trường**: luôn là **(C) nhiều máy/VM QNX qua mạng thật (Qnet)** —
  không có ngoại lệ trong file này. Nếu một nhóm chỉ có 1 VM, category này
  **không thể test được đúng nghĩa** và phải được ghi nhận là "chưa kiểm
  chứng" trong báo cáo, không được coi tương đương với chạy đa tiến trình
  trên cùng 1 node.
- **Cấu hình `TRAFFIC_NODE_MAP`**: giá trị mẫu chính xác (thay `VM1/VM2/VM3`
  bằng tên Qnet thật lấy từ lệnh `ls /net` chạy trên chính VM đó — xem mục
  2.2 `QNX_DEPLOYMENT_RUN_GUIDE.md`).
- **Chuẩn bị**: node nào chạy trên VM nào, thứ tự khởi động cụ thể cho case
  đó (một số case ở mục 5.7 cố tình phá vỡ "thứ tự khuyến nghị" để chứng
  minh hệ thống không phụ thuộc thứ tự).
- **Các bước**: thao tác tuần tự, có thể gồm lệnh operator console
  (`m`/`t`/`o`/`r`/`c`/`f`), lệnh shell (`kill`, `Ctrl+C`, khởi động lại
  binary), hoặc quan sát log.
- **Kết quả mong đợi**: mô tả chính xác dòng log/console/cột trạng thái sẽ
  xuất hiện, kèm chuỗi text thật (không diễn giải chung chung) để người test
  có thể `grep` trực tiếp.

## 2. Topology tham chiếu chuẩn (baseline) dùng cho toàn bộ file

Trừ khi test case nói khác, mọi test case trong file này mặc định dùng
topology sau — tương ứng **Case 2 "Two Computers"** trong root `README.md`
(2 máy vật lý) và mục 3 "Case 2" của `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`, chi
tiết hoá thành 3 VM (mỗi VM một vai trò, dễ theo dõi log/console hơn khi
viết test case) — PC A chạy 2 VM (Central + Intersection), PC B chạy 1 VM
(Railway):

| VM (tên gợi ý, thay bằng `ls /net` thật) | Máy vật lý | Chạy tiến trình | Ghi chú |
|---|---|---|---|
| `VM1` | PC A | `c_main` (không tham số) | Central — 1 tiến trình duy nhất |
| `VM2` | PC A | `lx_main 1`, `lx_main 2`, ..., `lx_main 6` (6 tiến trình, 6 SSH shell riêng hoặc background) | Intersection L1-L6 |
| `VM3` | PC B | `rlx_main 1`, `rlx_main 2`, `rlx_main 3` (3 tiến trình) | Railway RL1-RL3 |

`TRAFFIC_NODE_MAP` baseline — export trong **mỗi shell SSH** trước khi chạy
binary tương ứng (mỗi tiến trình chỉ cần các suffix mà chính nó chủ động gửi
tới qua `ipc_client_post()`; thừa không sao, thiếu thì message tới suffix đó
âm thầm không tới):

```sh
# Trên VM1 (c_main) — C1 gửi tới cả Lx (VM2) và RLx (VM3)
export TRAFFIC_NODE_MAP="l1=VM2,l2=VM2,l3=VM2,l4=VM2,l5=VM2,l6=VM2,rl1=VM3,rl2=VM3,rl3=VM3"
./c_main

# Trên VM2 (mỗi lx_main N) — Lx chỉ chủ động gửi HEARTBEAT lên C1 (VM1)
export TRAFFIC_NODE_MAP="c1=VM1"
./lx_main 1        # lặp lại cho 2..6 ở các shell khác, cùng export trên

# Trên VM3 (mỗi rlx_main N) — RLx gửi HEARTBEAT/FAULT_REPORT lên C1 (VM1)
# và CROSSING_STATUS tới 2 Lx liền kề (VM2) + C1 (VM1)
export TRAFFIC_NODE_MAP="c1=VM1,l1=VM2,l2=VM2,l3=VM2,l4=VM2,l5=VM2,l6=VM2"
./rlx_main 1        # lặp lại cho 2, 3 ở các shell khác, cùng export trên
```

Thứ tự khởi động "khuyến nghị" theo `README.md`/`QNX_DEPLOYMENT_RUN_GUIDE.md`
là C1 → RLx → Lx, nhưng mục 5.7 của file này kiểm chứng hệ thống **không
phụ thuộc** thứ tự đó.

## 3. Cách chạy từng node (nhắc lại nhanh)

- `./c_main` — không tham số (`app/central/src/c_main.c:126`, `main(void)`).
  Banner: `"C1 (Central Controller) starting..."` rồi
  `"C1: attached on traffic/c1, server loop starting."`.
- `./lx_main <1-6>` — chọn L1..L6 (`parse_self_id()`,
  `app/intersection/src/lx_main.c`). Sai/thiếu tham số:
  `"usage: %s <1-6>   (selects L1..L6)"`.
- `./rlx_main <1-3>` — chọn RL1..RL3 (`app/railway/src/rlx_main.c`). Sai/thiếu
  tham số: `"usage: %s <1-3>   (selects RL1..RL3)"`.

## 4. Nơi quan sát kết quả (log/console)

- **Central**: `c_logger_log()` (`app/central/src/c_logger.c`) ghi đồng thời
  ra **stdout** và file `central_log.txt` (mở bằng `fopen("central_log.txt", "a")`
  — đường dẫn **tương đối thư mục làm việc lúc chạy `c_main`**, `fflush`
  ngay sau mỗi dòng). Mỗi dòng có timestamp `[YYYY-MM-DD HH:MM:SS]`. Trên VM1:
  `tail -f central_log.txt` hoặc theo dõi trực tiếp terminal chạy `c_main`.
  Bảng trạng thái (`c_hmi.c`) in cột cuối đúng chuỗi `AVAILABLE` /
  `UNAVAILABLE`.
- **Lx/RLx**: **không ghi file log** — chỉ `printf`/`fprintf(stderr, ...)`
  thẳng ra console của chính tiến trình đó. Người test phải giữ terminal SSH
  của từng Lx/RLx mở để quan sát trực tiếp (không có cách xem lại sau khi
  đóng terminal).
- Chuỗi log thất bại gửi cần nhớ để `grep`/quan sát:
  - Central (qua `on_command_reply()`, `app/central/src/c_comm.c`):
    `"C1: <VERB> to <id> send failed (peer unreachable or send error)"`
    (VERB ví dụ `SET_TIMING_PROFILE`, `REQUEST_OVERRIDE`, ... theo
    `verb_name()`).
  - Lx (`app/intersection/src/lx_comm.c`):
    `"Lx: HEARTBEAT to <id> failed to send"` hoặc
    `"Lx: HEARTBEAT to C1 dropped - outgoing queue full or stopping"`.
  - RLx (`app/railway/src/rlx_comm.c`):
    `"RLx: message to <id> failed to send"`, hoặc cụ thể theo verb:
    `"RLx: HEARTBEAT to C1 dropped - outgoing queue full or stopping"`,
    `"RLx: FAULT_REPORT to C1 dropped - outgoing queue full or stopping"`,
    `"RLx: CROSSING_STATUS to <id> dropped - outgoing queue full or stopping"`.
  - Thay đổi đèn thật (mô phỏng bằng `printf`, `app/intersection/src/lx_signal.c`):
    `"Lx <id>: SIGNAL -> <TÊN PHA>"` (`ARTERIAL GREEN`, `ARTERIAL YELLOW`,
    `ALL RED (A to B)`, `CONNECTOR GREEN`, `CONNECTOR YELLOW`,
    `ALL RED (B to A)`), và khi override kết thúc:
    `"Lx <id>: override cleared/expired - running safe clearance sequence"`.

---

## 5. Danh sách test case

### 5.1 SET_TIMING_PROFILE cross-node (phím `t`)

#### TC-XNODE-01: Broadcast profile R1 tới L1/L3/L5 trên 3 VM riêng biệt
- **Loại**: Positive
- **Liên quan**: UC-03/SD-03, TC-01..TC-05, `c_mode_eng_get_chain(C_ARTERIAL_CHAIN_R1)` (`app/central/includes/c_mode_eng.h`: L1 offset=0ms, L3 offset=21000ms, L5 offset=45000ms, `LX_CYCLE_LENGTH_MS`=90000ms), `c_operator.c::handle_timing_profile()`, `lx_fsm_apply_offset_locked()` (`app/intersection/src/lx_fsm.c`).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: Topology mở rộng — để chứng minh triệt để
  (mỗi Lx đích một VM vật lý/ảo khác nhau, khác cả baseline mục 2 nơi
  L1-L6 dùng chung VM2). Nếu nhóm không đủ VM, có thể dùng baseline (L1, L3,
  L5 cùng chạy trên VM2) — vẫn là cross-node hợp lệ (C1 ↔ Lx khác VM), chỉ
  kém triệt để hơn ở việc chứng minh 3 Lx độc lập nhau; ghi rõ trường hợp nào
  được dùng trong báo cáo.

  ```sh
  # Trên VM1 (c_main)
  export TRAFFIC_NODE_MAP="l1=VM2A,l3=VM2B,l5=VM2C,rl1=VM3,rl2=VM3,rl3=VM3"
  ./c_main

  # Trên VM2A (chạy L1)
  export TRAFFIC_NODE_MAP="c1=VM1"
  ./lx_main 1

  # Trên VM2B (chạy L3)
  export TRAFFIC_NODE_MAP="c1=VM1"
  ./lx_main 3

  # Trên VM2C (chạy L5)
  export TRAFFIC_NODE_MAP="c1=VM1"
  ./lx_main 5
  ```
- **Chuẩn bị**: Trước khi bắt đầu, chạy `date` trên cả 4 VM (VM1, VM2A,
  VM2B, VM2C) và ghi lại độ lệch đồng hồ (nếu > vài giây, ghi chú vào kết quả
  vì ảnh hưởng trực tiếp tới `lx_fsm_apply_offset_locked()`). Khởi động C1
  trước (VM1), sau đó L1/L3/L5 (VM2A/B/C), theo thứ tự bất kỳ giữa 3 VM này.
  Đợi mỗi Lx in `"Lx <n>: attached on traffic/l<n>, server loop starting."`
  trước khi qua bước tiếp theo. Đặt cả 3 Lx ở `MODE_PEAK_FIXED` trước (dùng
  lệnh `m` nếu mode mặc định không phải PEAK_FIXED).
- **Các bước**:
  1. Trên console operator của C1 (VM1), nhấn `t`.
  2. Nhập `1` khi được hỏi `"chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): "`.
  3. Quan sát log Central.
  4. Quan sát console của L1 (VM2A), L3 (VM2B), L5 (VM2C).
  5. Ghi lại thời điểm wall-clock (giờ hệ thống của từng VM, dùng `date`)
     tại thời điểm mỗi Lx in `"SIGNAL -> ARTERIAL GREEN"` ở lần bắt đầu pha
     xanh liên tuyến **kế tiếp** sau khi nhận profile (không phải pha đang
     chạy dở — theo thiết kế, offset chỉ áp dụng tại ranh giới pha mới).
- **Kết quả mong đợi**:
  - `central_log.txt`/stdout VM1 có dòng
    `"Operator: SET_TIMING_PROFILE broadcast (chain=R1, profile_id=<N>) submitted"`.
  - Cả 3 dòng phản hồi ACK xuất hiện, dạng
    `"C1: SET_TIMING_PROFILE to <id> -> ACK"` cho id = L1, L3, L5 (id số
    theo `controller_id_t`, không phải "L1"/"L3"/"L5" dạng chữ).
  - Không có dòng nào `"send failed"` hay `NACK` cho 3 target này.
  - Thời điểm bắt đầu `ARTERIAL GREEN` của L3 lệch sau L1 xấp xỉ 21 giây, và
    L5 lệch sau L1 xấp xỉ 45 giây (± sai số đồng hồ đã ghi ở bước Chuẩn bị,
    ± tối đa 1 chu kỳ 90s nếu Lx đang giữa pha xanh khi nhận profile — đây là
    giới hạn thiết kế đã biết, không phải lỗi).

#### TC-XNODE-02: Broadcast profile R2 tới L2/L4/L6 trên 3 VM riêng biệt
- **Loại**: Positive
- **Liên quan**: giống TC-XNODE-01 nhưng chain R2
  (`C_ARTERIAL_CHAIN_R2`: L2 offset=0ms, L4 offset=19000ms, L6 offset=42000ms).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: tương tự TC-XNODE-01, thay `l1/l3/l5` bằng
  `l2/l4/l6` và `./lx_main 1/3/5` bằng `./lx_main 2/4/6`.
- **Chuẩn bị**: giống TC-XNODE-01, dùng L2/L4/L6 thay vì L1/L3/L5.
- **Các bước**: giống TC-XNODE-01, ở bước 2 nhập `2` (chain R2).
- **Kết quả mong đợi**: 3 ACK cho L2, L4, L6; L4 bắt đầu `ARTERIAL GREEN`
  lệch sau L2 khoảng 19 giây; L6 lệch sau L2 khoảng 42 giây (cùng lưu ý sai
  số đồng hồ/1-chu-kỳ như TC-XNODE-01).

#### TC-XNODE-03: Broadcast timing profile khi 1 Lx đích chưa khởi động
- **Loại**: Negative
- **Liên quan**: `ipc_client_post()`/`ipc_client_thread_main()` graceful
  failure path (`app/shared/src/qnet_utils.c`), `c_comm.c::on_command_reply()`.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: dùng baseline (mục 2): VM1=C1, VM2=L1-L6,
  VM3=RL1-RL3.
- **Chuẩn bị**: Khởi động C1 (VM1). Trên VM2, chỉ khởi động L1 và L3
  (`./lx_main 1`, `./lx_main 3`); **cố tình chưa chạy** `./lx_main 5`.
- **Các bước**:
  1. Trên C1, nhấn `t`, chọn chain `1` (R1: L1, L3, L5).
  2. Quan sát log Central trong vài giây.
  3. Sau đó mới khởi động `./lx_main 5` trên VM2.
  4. Quan sát xem L5 có tự nhận được profile mà không cần C1 gửi lại không.
- **Kết quả mong đợi**:
  - L1, L3 nhận ACK bình thường: `"C1: SET_TIMING_PROFILE to <L1 id> -> ACK"`,
    `"C1: SET_TIMING_PROFILE to <L3 id> -> ACK"`.
  - Riêng L5: `"C1: SET_TIMING_PROFILE to <L5 id> send failed (peer unreachable or send error)"`.
  - Central **không crash, không treo**, hai request kia vẫn xử lý bình
    thường (mỗi `ipc_client_post()` độc lập theo từng target trong vòng lặp
    broadcast — 1 lần gọi cho mỗi phần tử chain).
  - Sau khi L5 khởi động ở bước 3, L5 **không tự động nhận lại** profile đã
    broadcast trước đó (không có cơ chế retry/resend trong code) — operator
    phải phát lệnh `t` lại nếu muốn L5 đồng bộ. Đây là hành vi đúng theo
    thiết kế hiện tại, cần ghi nhận rõ trong test report như một giới hạn
    (không phải bug cần fix trong phạm vi test này).

### 5.2 REQUEST_OVERRIDE / RENEW_OVERRIDE / CANCEL_OVERRIDE cross-node (phím `o`/`r`/`c`)

#### TC-XNODE-04: REQUEST_OVERRIDE arterial trên Lx ở VM khác — đèn đổi màu thật
- **Loại**: Positive
- **Liên quan**: UC-08/SD-07, `c_operator.c::handle_request_override()`,
  `lx_fsm.c::lx_fsm_on_request_override()`, `lx_fsm_advance_phase_locked()`
  (áp dụng `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN` tại ranh giới
  `ALL_RED` kế tiếp), `lx_signal.c::lx_signal_show_phase()`. Đây là tính năng
  **vừa sửa xong** theo yêu cầu — phải test kỹ, không chỉ kiểm tra ACK.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Khởi động C1 (VM1), sau đó L2 trên VM2 (`./lx_main 2`), đảm
  bảo L2 không ở `SUPERVISORY_CENTRAL_OVERRIDE`/`SUPERVISORY_RAILWAY_PREEMPTION`/
  `SUPERVISORY_FAULT_SAFE` và không có `ped_clearance_active` (mode mặc định,
  không có phương tiện/người đi bộ giả lập gây pha ped đang chạy — nếu
  `lx_sensor` giả lập có ped request, hủy trước).
- **Các bước**:
  1. Trên C1, nhấn `o`.
  2. Nhập `2` (Lx number).
  3. Nhập `0` (target movement = arterial).
  4. Nhập một `duration_ms` hợp lệ, ví dụ `60000` (≤ 300000 theo giới hạn
     `LX_OVERRIDE_DURATION_CAP_MS`).
  5. Quan sát log Central ngay lập tức.
  6. Giữ terminal L2 (VM2) mở, quan sát cho tới khi qua ranh giới `ALL_RED`
     kế tiếp (tối đa một chu kỳ đèn).
- **Kết quả mong đợi**:
  - Central: `"Operator: REQUEST_OVERRIDE(target=<L2 id>, movement=0, duration_ms=60000) submitted"`
    rồi `"C1: REQUEST_OVERRIDE to <L2 id> -> ACK"` (không phải `ACK_PENDING`
    vì không có ped clearance đang chạy).
  - Trên console L2 (VM2), **thứ tự log thật quan sát được** là: L2 vẫn
    chạy hết pha hiện tại bình thường, sau đó khi tới ranh giới `ALL_RED`,
    xuất hiện `"Lx 2: SIGNAL -> ALL RED (A to B)"` (hoặc `(B to A)`, tuỳ
    ranh giới nào tới trước), **ngay sau đó** là
    `"Lx 2: SIGNAL -> ARTERIAL GREEN"` — đây chính là bằng chứng đèn đã đổi
    theo lệnh override xuyên node (không phải chỉ ACK ở tầng IPC).
  - Việc ACK ở bước Central xảy ra gần như tức thời (< 1s), nhưng đèn đổi
    màu thật ở L2 có thể trễ tới hết pha hiện tại — ghi rõ độ trễ quan sát
    được trong báo cáo, không coi là lỗi.

#### TC-XNODE-05: REQUEST_OVERRIDE connector trên Lx ở VM khác
- **Loại**: Positive
- **Liên quan**: giống TC-XNODE-04, `target_movement = OVERRIDE_MOVEMENT_CONNECTOR (1)`.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: giống TC-XNODE-04, có thể dùng L4 (VM2) thay vì L2 để tách
  biệt với TC-XNODE-04 nếu chạy nối tiếp trong cùng buổi test.
- **Các bước**: giống TC-XNODE-04 nhưng ở bước 3 nhập `1` (connector).
- **Kết quả mong đợi**: Console L4 in
  `"Lx 4: SIGNAL -> ALL RED (...)"` rồi `"Lx 4: SIGNAL -> CONNECTOR GREEN"`
  (khác pha với TC-XNODE-04) — xác nhận `target_movement` truyền đúng xuyên
  node, không bị Lx diễn giải sai/mặc định về arterial.

#### TC-XNODE-06: REQUEST_OVERRIDE bị NACK vì railway đang preemption — đèn không đổi
- **Loại**: Negative
- **Liên quan**: `lx_fsm_on_request_override()` nhánh
  `fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION` → `RESULT_NACK`,
  `NACK_REASON_RAILWAY_CONFLICT`; RC-02/CC-02.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật (cần cả RLx thật để tạo
  điều kiện preemption qua Qnet, không giả lập tại chỗ).
- **Cấu hình TRAFFIC_NODE_MAP**: baseline đầy đủ 3 VM (mục 2) — cần RL1
  (VM3) thật sự gửi `CROSSING_STATUS` tới L1 (VM2) để đưa L1 vào
  `SUPERVISORY_RAILWAY_PREEMPTION`.
- **Chuẩn bị**: Khởi động đủ C1 (VM1), L1+L2 (VM2), RL1 (VM3). Kích hoạt
  RL1 chuyển sang trạng thái báo tàu đang tới/occupied (theo sensor giả lập
  của RLx, xem file test-plan RLx nội bộ để biết cách kích) sao cho L1 nhận
  được `CROSSING_STATUS` khiến `fsm->supervisory = SUPERVISORY_RAILWAY_PREEMPTION`.
  Xác nhận qua console L1 rằng preemption đã áp dụng trước khi qua bước tiếp.
- **Các bước**:
  1. Trên C1, nhấn `o`, chọn Lx `1`, movement bất kỳ (`0`), duration hợp lệ
     (`60000`).
  2. Quan sát log Central.
  3. Quan sát console L1 — xác nhận không có dòng `SIGNAL ->` mới nào phát
     sinh do lệnh override này.
- **Kết quả mong đợi**:
  - Central: `"C1: REQUEST_OVERRIDE to <L1 id> -> NACK reason=RAILWAY_CONFLICT"`.
  - Console L1 không in thêm dòng `SIGNAL ->` nào ngoài các dòng đã do
    preemption railway gây ra trước đó — đèn giữ nguyên trạng thái an toàn
    do railway preemption quyết định, override của Central bị từ chối hoàn
    toàn (không có hiệu lực một phần).

#### TC-XNODE-07: REQUEST_OVERRIDE trong lúc ped clearance đang chạy — ACK_PENDING rồi mới đổi đèn
- **Loại**: Edge case
- **Liên quan**: `lx_fsm_on_request_override()` nhánh
  `fsm->ped_clearance_active` → `OVR_PENDING_CLEARANCE`, `RESULT_ACK_PENDING`;
  đèn chỉ đổi sau khi `lx_fsm_on_phase_timer()` re-validate và chuyển
  `OVR_ACTIVE`.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Khởi động C1 (VM1), L3 (VM2). Kích hoạt một yêu cầu người đi
  bộ (ped request) trên L3 ngay trước khi gửi override, sao cho
  `ped_clearance_active = 1` tại thời điểm C1 gửi lệnh (canh thời gian theo
  chu kỳ pha ped của L3).
- **Các bước**:
  1. Ngay khi L3 đang trong pha ped clearance (quan sát console L3 in
     `PED SIGNAL ... FLASHING_DONT_WALK`/`DONT_WALK`), trên C1 nhấn `o`,
     chọn Lx `3`, movement `1` (connector), duration `60000`.
  2. Quan sát log Central ngay lập tức.
  3. Tiếp tục giữ console L3 mở tới khi ped clearance kết thúc và qua ranh
     giới ALL_RED kế tiếp.
- **Kết quả mong đợi**:
  - Central: `"C1: REQUEST_OVERRIDE to <L3 id> -> ACK_PENDING"` (khác hẳn
    `ACK` thẳng của TC-XNODE-04/05) — xác nhận trạng thái pending truyền
    đúng qua Qnet, không bị Central/console hiểu nhầm thành ACK thường.
  - Đèn L3 **không đổi ngay** khi nhận `ACK_PENDING` — vẫn hoàn tất pha ped
    clearance như bình thường.
  - Sau khi ped clearance kết thúc, override tự chuyển `OVR_ACTIVE` và đèn
    L3 đổi sang `CONNECTOR GREEN` tại ranh giới ALL_RED kế tiếp — quan sát
    dòng `"Lx 3: SIGNAL -> CONNECTOR GREEN"` xuất hiện **sau** khi ped
    clearance đã hoàn tất, không phải ngay sau ACK_PENDING.

#### TC-XNODE-08: RENEW_OVERRIDE rồi CANCEL_OVERRIDE cross-node
- **Loại**: Positive
- **Liên quan**: UC-08/SD-07/BR-7, `c_operator.c::handle_renew_override()`
  /`handle_cancel_override()`, `lx_fsm.c::lx_fsm_terminate_override_locked()`,
  `lx_signal.c::lx_signal_show_override_clearance()`.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Chạy lại TC-XNODE-04 trước (L2 đang override arterial,
  duration ngắn ví dụ `15000` ms để dễ quan sát renew/cancel trong buổi
  test).
- **Các bước**:
  1. Trước khi 15s hết hạn, trên C1 nhấn `r`, chọn Lx `2`,
     `extend_duration_ms` = `30000`.
  2. Quan sát log Central xác nhận renew được gửi.
  3. Sau đó nhấn `c`, chọn Lx `2` để hủy override sớm.
  4. Quan sát console L2.
- **Kết quả mong đợi**:
  - Bước 2: `"C1: RENEW_OVERRIDE to <L2 id> -> ACK"` (không bị NACK vì
    override đang active hợp lệ).
  - Bước 3: `"Operator: CANCEL_OVERRIDE(target=<L2 id>) submitted"` rồi
    `"C1: CANCEL_OVERRIDE to <L2 id> -> ACK"`.
  - Console L2: `"Lx 2: override cleared/expired - running safe clearance sequence"`
    xuất hiện ngay sau khi nhận CANCEL_OVERRIDE xuyên node — xác nhận lệnh
    hủy từ VM khác có tác dụng thật trên thiết bị, không chỉ dừng ở tầng ACK.

### 5.3 SET_MODE và REQUEST_FAULT_CLEAR cross-node (phím `m`/`f`)

#### TC-XNODE-09: SET_MODE cross-node, phản ánh qua HEARTBEAT kế tiếp
- **Loại**: Positive
- **Liên quan**: UC-07/SD-03, `c_operator.c::handle_set_mode()`,
  `lx_comm.c::lx_comm_send_heartbeat()` (heartbeat mang theo `mode` hiện tại
  qua `lx_fsm_fill_status()`), bảng trạng thái `c_hmi.c`.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Khởi động C1 (VM1), L6 (VM2). Xác nhận mode hiện tại của L6
  trên bảng trạng thái Central (mặc định thường là `OFF_PEAK_SENSOR`).
- **Các bước**:
  1. Trên C1, nhấn `m`, chọn Lx `6`, mode `0` (PEAK_FIXED).
  2. Đợi ít nhất 1-2 giây (một chu kỳ heartbeat) để L6 gửi HEARTBEAT tiếp
     theo mang mode mới.
  3. Xem lại bảng trạng thái Central (`c_hmi.c` render).
- **Kết quả mong đợi**:
  - Central: `"Operator: SET_MODE(target=<L6 id>, mode=PEAK_FIXED) submitted"`
    rồi `"C1: SET_MODE to <L6 id> -> ACK"`.
  - Cột mode của L6 trong bảng trạng thái Central chuyển sang phản ánh
    `PEAK_FIXED` sau khi heartbeat kế tiếp tới — xác nhận trạng thái mode
    thay đổi thật trên node ở VM khác, không chỉ ACK ở tầng lệnh.

#### TC-XNODE-10: REQUEST_FAULT_CLEAR cross-node tới RLx
- **Loại**: Positive
- **Liên quan**: UC-06 alt 7.1/SD-06, RC-09, `c_operator.c::handle_request_fault_clear()`.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Khởi động C1 (VM1), RL2 (VM3). Đưa RL2 vào trạng thái có
  fault (kích sensor lỗi giả lập theo tài liệu test nội bộ RLx) để có gì đó
  để clear.
- **Các bước**:
  1. Trên C1, nhấn `f`, chọn RLx `2`.
  2. Quan sát log Central.
  3. Quan sát console RL2 (VM3).
- **Kết quả mong đợi**:
  - Central: `"Operator: REQUEST_FAULT_CLEAR(target=<RL2 id>) submitted"`
    rồi `"C1: REQUEST_FAULT_CLEAR to <RL2 id> -> ACK"` (hoặc `NACK` nếu điều
    kiện clear chưa hợp lệ — ghi rõ reason nếu xảy ra).
  - Nếu ACK: fault flag của RL2 được xóa, quan sát được qua
    `FAULT_REPORT` kế tiếp của RL2 lên Central (fault_code giảm/về 0) —
    xác nhận lệnh clear xuyên node có hiệu lực thật trên RLx, đúng RC-09
    ("Central có thể yêu cầu clear fault nhưng không được tự thao tác thiết
    bị đường sắt" — RLx tự quyết định và thực thi việc clear).

### 5.4 CROSSING_STATUS broadcast tới 3 nơi (RLx → 2 Lx liền kề + C1)

#### TC-XNODE-11: RL1 báo CROSSING_STATUS — cả L1, L2, C1 đều nhận
- **Loại**: Positive
- **Liên quan**: RC-01/RC-02/SD-04/Diagram 4,
  `rlx_comm.c::rlx_comm_broadcast_crossing_status_if_changed()`, adjacency
  RL1→{L1,L2}.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2), đảm bảo RL1 trên VM3 có
  `l1=VM2,l2=VM2` (đã có sẵn trong map baseline).
- **Chuẩn bị**: Khởi động đủ C1 (VM1), L1+L2 (VM2), RL1 (VM3), theo thứ tự
  bất kỳ nhưng đợi cả 3 in banner "attached" trước khi bắt đầu.
- **Các bước**:
  1. Kích hoạt sensor giả lập trên RL1 để chuyển `crossing_state_t` (ví dụ
     từ CLEAR sang WARNING) — theo cách kích của RLx sensor console.
  2. Ngay lập tức quan sát cả 3 console: RL1 (VM3), L1 (VM2), L2 (VM2), và
     log Central (VM1).
- **Kết quả mong đợi**:
  - Console RL1 không báo lỗi gửi (`"RLx: message to ... failed to send"`
    hay `"CROSSING_STATUS to ... dropped"` **không** xuất hiện).
  - Log Central có dòng ghi nhận CROSSING_STATUS từ RL1 (qua
    `c_server_record_crossing_status()` reset heartbeat, quan sát gián tiếp
    qua bảng trạng thái RL1 vẫn `AVAILABLE`/`missed_heartbeat_ticks` về 0).
  - Cả L1 và L2 (2 tiến trình độc lập trên VM2) đều phản ứng với trạng thái
    mới (ví dụ chuyển `supervisory` sang `SUPERVISORY_RAILWAY_PREEMPTION`
    nếu state là WARNING/OCCUPIED — xem test-plan nội bộ Lx để biết dấu hiệu
    quan sát chính xác). **Cả 3 nơi phải nhận — nếu chỉ 1 hoặc 2 trong 3
    phản ứng, test FAIL** (đây chính là điều category này muốn chứng minh
    chặt hơn test đơn lẻ).

#### TC-XNODE-12: RL1 báo CROSSING_STATUS khi L2 chưa khởi động — graceful, 2/3 vẫn nhận
- **Loại**: Negative / Edge case
- **Liên quan**: giống TC-XNODE-11, cộng graceful-failure path của
  `ipc_client_post()`/`ipc_client_thread_main()`.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Khởi động C1 (VM1), RL1 (VM3), và **chỉ** L1 trên VM2
  (`./lx_main 1`) — cố tình **chưa** chạy `./lx_main 2`.
- **Các bước**:
  1. Kích hoạt sensor RL1 để đổi crossing state (như TC-XNODE-11 bước 1).
  2. Quan sát console RL1, console L1, log Central.
  3. Sau đó mới khởi động `./lx_main 2` trên VM2.
  4. Kích hoạt thêm một lần đổi trạng thái khác trên RL1 (ví dụ WARNING →
     CLEAR) để tạo sự kiện mới.
- **Kết quả mong đợi**:
  - Bước 2: console RL1 in đúng
    `"RLx: CROSSING_STATUS to <L2 id> dropped - outgoing queue full or stopping"`
    **CHỈ KHI** hàng đợi đầy — trường hợp bình thường (L2 chưa chạy, tên Qnet
    không tồn tại) thực chất đi qua nhánh `name_open()` thất bại ở
    `ipc_client_thread_main()`, dẫn tới `on_reply_log_failure()` với
    `send_ok = 0`, in ra
    **`"RLx: message to <L2 id> failed to send"`** (đây là log thực sự cần
    tìm — ghi rõ để tránh nhầm 2 dạng lỗi với nhau). L1 và C1 vẫn nhận bình
    thường, không có gì bất thường ở 2 nơi đó.
  - RL1 **không treo, không crash**, tiếp tục vòng lặp bình thường
    (nhờ luồng client riêng, non-blocking `ipc_client_post()`).
  - Bước 3-4: sau khi L2 khởi động, L2 sẽ nhận **CROSSING_STATUS của lần đổi
    trạng thái tiếp theo** — nó **không** nhận lại được trạng thái đã bị bỏ
    lỡ trước đó, vì `rlx_comm_broadcast_crossing_status_if_changed()` chỉ
    gửi khi state thực sự đổi so với `last_broadcast_state` (không có
    resend/backfill). Ghi rõ giới hạn này trong báo cáo test: nếu L2 khởi
    động muộn ngay trước một cửa sổ nguy hiểm, nó có thể không biết trạng
    thái crossing hiện tại cho tới lần đổi trạng thái kế tiếp — đây là rủi ro
    thiết kế cần nêu, không phải phạm vi sửa của test này.

### 5.5 Node down / watchdog PA-07 (kill và khởi động lại)

#### TC-XNODE-13: Tắt đột ngột 1 Lx — Central đánh dấu UNAVAILABLE sau đúng 3 giây
- **Loại**: Positive
- **Liên quan**: PA-07, `c_watchdog_mon.c::c_watchdog_mon_tick()` (tick 1
  giây qua `IPC_PULSE_HEARTBEAT_TICK`), `c_hmi.c` (cột `AVAILABILITY`).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Khởi động đủ C1 (VM1), toàn bộ L1-L6 (VM2), RL1-RL3 (VM3).
  Đợi bảng trạng thái Central hiển thị cả 9 controller `AVAILABLE`.
- **Các bước**:
  1. Trên VM2, chọn 1 tiến trình, ví dụ `lx_main 4` (L4), bấm `Ctrl+C`
     (hoặc `kill <pid>` từ shell khác) để tắt đột ngột, không qua trình tự
     tắt sạch (nếu có).
  2. Bấm giờ ngay lúc tắt.
  3. Theo dõi liên tục `central_log.txt`/stdout VM1 trong 5 giây tiếp theo.
  4. Đồng thời quan sát các Lx/RLx còn lại (VM2 các tiến trình khác, VM3) —
     xác nhận chúng vẫn hoạt động bình thường (heartbeat đều, không log lỗi
     lạ).
- **Kết quả mong đợi**:
  - Đúng ~3 giây sau lần heartbeat cuối cùng của L4 (tức ~3 tick của
    `IPC_PULSE_HEARTBEAT_TICK`), Central log:
    `"Controller <L4 id> marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"`.
  - Bảng trạng thái Central: dòng L4 chuyển cột `AVAILABILITY` từ
    `AVAILABLE` sang `UNAVAILABLE`; các dòng khác (L1,L2,L3,L5,L6,RL1,RL2,RL3)
    vẫn `AVAILABLE`.
  - Các tiến trình còn lại **không** bị treo/crash/log lỗi bất thường do sự
    kiện này — chứng minh 1 node chết không kéo sập hệ thống phân tán.
  - Nếu L4 đang là 1 trong 2 Lx liền kề của 1 RLx (ví dụ RL2 kề L3/L4), RLx
    đó khi gửi CROSSING_STATUS tới L4 trong lúc L4 đã chết sẽ log
    `"RLx: message to <L4 id> failed to send"` — đây là hành vi đúng theo
    thiết kế graceful-failure, không phải lỗi mới.

#### TC-XNODE-14: Tắt đột ngột 1 RLx giữa lúc crossing đang WARNING — trạng thái đóng băng, không crash
- **Loại**: Edge case
- **Liên quan**: PA-07, RC-02/CC-02 (Lx phụ thuộc trạng thái crossing cuối
  cùng nhận được để quyết định suppress movement).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: Khởi động đủ hệ thống. Kích RL2 chuyển sang WARNING (ảnh
  hưởng L3, L4 theo adjacency).
- **Các bước**:
  1. Xác nhận L3, L4 đã ở `SUPERVISORY_RAILWAY_PREEMPTION` (do WARNING của
     RL2).
  2. Trên VM3, `kill` tiến trình `rlx_main 2` (RL2) đột ngột.
  3. Theo dõi Central trong 5 giây tiếp theo.
  4. Quan sát console L3, L4 — chúng có tự thoát khỏi preemption hay giữ
     nguyên trạng thái an toàn cuối cùng đã biết?
- **Kết quả mong đợi**:
  - Sau ~3 giây, Central log
    `"Controller <RL2 id> marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"`,
    bảng trạng thái RL2 chuyển `UNAVAILABLE`.
  - L3, L4 **giữ nguyên** trạng thái preemption đã biết cuối cùng (không tự
    ý coi RL2 mất kết nối là "an toàn để mở lại giao thông") — vì Lx không
    có kết nối trực tiếp tới trạng thái AVAILABLE/UNAVAILABLE của RLx trên
    Central, nó chỉ dựa vào `CROSSING_STATUS` cuối cùng nhận được. Ghi nhận
    đây là hành vi an toàn hợp lý (fail-safe: mất tín hiệu → giữ trạng thái
    hạn chế cuối cùng thay vì mở lại) chứ không phải bug, nhưng cũng ghi rõ
    nếu code có timeout tự-clear thì mô tả timeout đó (nếu không tìm thấy
    trong code, ghi "không có cơ chế tự hết hạn — cần RL2 khởi động lại và
    gửi state mới để giải phóng").
  - Central và các Lx/RLx khác không crash/treo do RL2 biến mất.

#### TC-XNODE-15: Khởi động lại node đã tắt — tự nhận AVAILABLE qua heartbeat tiếp theo
- **Loại**: Positive
- **Liên quan**: PA-07, `c_server.c::c_server_record_status()` (reset
  `missed_heartbeat_ticks = 0`, `marked_unavailable = 0` ngay khi nhận bất kỳ
  message hợp lệ nào — HEARTBEAT/STATUS/CROSSING_STATUS).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2) — **dùng đúng biến môi
  trường đã export trước đó trong cùng shell**, hoặc export lại y hệt nếu mở
  shell SSH mới (không được quên bước này khi khởi động lại, nếu không sẽ
  vô tình rơi vào kịch bản TC-XNODE-17).
- **Chuẩn bị**: Tiếp nối trực tiếp từ TC-XNODE-13 (L4 vừa bị tắt và đã được
  đánh dấu UNAVAILABLE).
- **Các bước**:
  1. Trên VM2, chạy lại `export TRAFFIC_NODE_MAP="c1=VM1"` (nếu shell mới)
     rồi `./lx_main 4`.
  2. Đợi L4 in banner attach thành công.
  3. Theo dõi bảng trạng thái Central trong tối đa 2 giây sau heartbeat đầu
     tiên của L4.
- **Kết quả mong đợi**:
  - Bảng trạng thái Central: dòng L4 chuyển từ `UNAVAILABLE` về `AVAILABLE`
    ngay tại heartbeat hợp lệ đầu tiên nhận được từ L4 — **không cần đợi đủ
    3 tick liên tiếp** (khác chiều với việc đánh dấu unavailable, vốn cần 3
    tick; việc phục hồi chỉ cần 1 heartbeat vì logic reset nằm ở
    `c_server_record_status()`, chạy ngay khi nhận message, không đợi tick).
  - **Không có dòng log riêng nào báo "AVAILABLE trở lại"** — đây là hành vi
    đúng theo code hiện tại (chỉ có log khi chuyển sang UNAVAILABLE); người
    test phải xác nhận qua cột `AVAILABILITY` của bảng trạng thái, không
    phải qua log text. Ghi rõ điều này trong test report để tránh hiểu nhầm
    là "thiếu log".

### 5.6 Cấu hình `TRAFFIC_NODE_MAP` sai

#### TC-XNODE-16: TRAFFIC_NODE_MAP trỏ tới tên Qnet node không tồn tại
- **Loại**: Negative
- **Liên quan**: `build_open_path()`/`name_open()` trả `-1`,
  `ipc_client_thread_main()`'s `send_ok = 0` path (`app/shared/src/qnet_utils.c`).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: **cố tình sai** trên VM1:
  ```sh
  # Trên VM1 — "l4" trỏ tới một tên Qnet node không có thật trên mạng
  export TRAFFIC_NODE_MAP="l1=VM2,l2=VM2,l3=VM2,l4=VM_KHONG_TON_TAI,l5=VM2,l6=VM2,rl1=VM3,rl2=VM3,rl3=VM3"
  ./c_main
  ```
  Các VM còn lại dùng cấu hình baseline bình thường (mục 2).
- **Chuẩn bị**: Khởi động đủ hệ thống, bao gồm L4 thật trên VM2
  (`./lx_main 4`) — tức là L4 **có tồn tại và đang chạy đúng**, chỉ có
  `TRAFFIC_NODE_MAP` trên VM1 (phía C1) khai sai tên node.
- **Các bước**:
  1. Trên C1, nhấn `m`, chọn Lx `4`, mode bất kỳ.
  2. Quan sát log Central.
  3. Thử tiếp một lệnh khác tới Lx đúng (ví dụ `m` chọn Lx `1`, cấu hình
     đúng) ngay sau đó.
- **Kết quả mong đợi**:
  - Central **không crash, không treo tiến trình operator hay client
    thread**.
  - Log Central: `"C1: SET_MODE to <L4 id> send failed (peer unreachable or send error)"`
    — vì `name_open("/net/VM_KHONG_TON_TAI/dev/name/global/traffic/l4", 0)`
    trả `-1` (Qnet không tìm thấy node đó), dẫn thẳng tới nhánh `send_ok = 0`.
  - Lệnh tới L1 ở bước 3 (cấu hình đúng) vẫn thành công bình thường
    (`"C1: SET_MODE to <L1 id> -> ACK"`) — xác nhận 1 entry sai trong map
    không ảnh hưởng các entry khác, đúng theo thiết kế parse-từng-entry độc
    lập của `node_map_load()`.

#### TC-XNODE-17: TRAFFIC_NODE_MAP thiếu entry cho một peer thực sự ở xa
- **Loại**: Negative
- **Liên quan**: giống TC-XNODE-16 nhưng qua đường khác —
  `resolve_node()` trả `NULL` (suffix vắng mặt) khiến `build_open_path()`
  dùng đường same-node (`"traffic/l4"`, không có `/net/` prefix), trong khi
  L4 thực tế nằm trên VM khác — `name_open()` cũng trả `-1` nhưng vì lý do
  khác (tìm trong namespace local của chính VM1, không tìm thấy vì tên đó
  chỉ tồn tại global trên VM2).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: **cố tình thiếu** entry `l4` trên VM1:
  ```sh
  # Trên VM1 — quên hẳn "l4=..." trong danh sách
  export TRAFFIC_NODE_MAP="l1=VM2,l2=VM2,l3=VM2,l5=VM2,l6=VM2,rl1=VM3,rl2=VM3,rl3=VM3"
  ./c_main
  ```
  Các VM còn lại dùng baseline bình thường.
- **Chuẩn bị**: giống TC-XNODE-16 (L4 thật, đang chạy đúng trên VM2).
- **Các bước**: giống TC-XNODE-16 (lệnh `m` tới Lx `4`, rồi lệnh đúng tới
  Lx `1`).
- **Kết quả mong đợi**:
  - Cùng kết quả về mặt hành vi cuối cùng với TC-XNODE-16 (gửi thất bại,
    log `"send failed (peer unreachable or send error)"`, hệ thống không
    crash, lệnh khác không bị ảnh hưởng) — nhưng nguyên nhân gốc khác nhau
    (thiếu entry → coi là same-node, không phải sai tên node). Ghi rõ trong
    báo cáo test rằng **cả hai lỗi cấu hình khác nhau đều dẫn tới cùng một
    kiểu graceful failure ở tầng ứng dụng** — đây là điểm mạnh cần nêu bật
    (không có class lỗi cấu hình nào trong 2 loại này làm crash hệ thống).

### 5.7 Khởi động các node theo thứ tự bất kỳ

#### TC-XNODE-18: Khởi động ngược thứ tự khuyến nghị (Lx trước, RLx giữa, C1 cuối cùng)
- **Loại**: Positive
- **Liên quan**: xác nhận thứ tự "C1 → RLx → Lx" nêu trong
  `README.md`/`docs/QNX_DEPLOYMENT_RUN_GUIDE.md` mục 2.3 chỉ là khuyến nghị
  vận hành, **không phải yêu cầu bắt buộc về mặt kỹ thuật** — mỗi node tự
  `name_attach()` độc lập, không chờ đợi lẫn nhau lúc khởi động, và
  `ipc_client_post()` luôn graceful nếu peer chưa sẵn sàng (giống cơ chế đã
  kiểm chứng ở mục 5.4/5.6).
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2) — không đổi gì so với
  cấu hình chuẩn, export như bình thường trên cả 3 VM **trước khi** chạy
  binary theo thứ tự mới ở bước dưới.
- **Chuẩn bị**: Đảm bảo không có tiến trình `c_main`/`lx_main`/`rlx_main`
  nào đang chạy trên cả 3 VM (dừng sạch từ lần test trước).
- **Các bước**:
  1. Trên VM2: khởi động toàn bộ `./lx_main 1` .. `./lx_main 6` (Lx khởi
     động **đầu tiên**, trước cả C1 và RLx).
  2. Đợi 5 giây (để chắc chắn các heartbeat đầu tiên của Lx gửi đi đều thất
     bại một cách graceful vì C1 chưa tồn tại) — kiểm tra console mỗi Lx có
     in `"Lx: HEARTBEAT to C1 dropped - outgoing queue full or stopping"`
     hoặc `"Lx: HEARTBEAT to <C1 id> failed to send"` mà **không** crash.
  3. Trên VM3: khởi động `./rlx_main 1`, `./rlx_main 2`, `./rlx_main 3`
     (RLx khởi động **thứ hai**).
  4. Đợi 5 giây, xác nhận RLx cũng không crash dù C1 chưa tồn tại (heartbeat
     của RLx thất bại graceful tương tự).
  5. Cuối cùng, trên VM1: khởi động `./c_main`.
  6. Theo dõi bảng trạng thái Central trong 5 giây đầu sau khi C1 khởi động.
- **Kết quả mong đợi**:
  - Không có tiến trình nào (Lx, RLx) crash hay treo trong lúc chờ C1 chưa
    tồn tại — mọi lần gửi heartbeat thất bại đều log graceful như mô tả ở
    bước 2/4, tiến trình vẫn tiếp tục vòng lặp heartbeat 1 giây tiếp theo.
  - Ngay sau khi C1 khởi động và attach xong (`"C1: attached on traffic/c1,
    server loop starting."`), heartbeat hợp lệ tiếp theo từ **mỗi** node
    (trong vòng tối đa ~1 giây/node do chu kỳ heartbeat) khiến bảng trạng
    thái Central điền đủ cả 9 dòng `AVAILABLE` — **không cần khởi động lại
    bất kỳ Lx/RLx nào** đã lỡ khởi động trước C1.
  - Gửi thử một lệnh operator (ví dụ `m` tới Lx `1`) ngay sau khi bảng trạng
    thái đầy đủ — xác nhận hệ thống hoạt động bình thường 100%, y hệt như
    khi khởi động đúng thứ tự khuyến nghị.

#### TC-XNODE-19: Khởi động thứ tự ngẫu nhiên khác (RLx trước, Lx giữa, C1 cuối; xen kẽ)
- **Loại**: Positive
- **Liên quan**: giống TC-XNODE-18, kiểm chứng thêm một hoán vị khác để
  loại trừ khả năng TC-XNODE-18 "may mắn" đúng nhờ thứ tự cụ thể đó.
- **Môi trường**: (C) nhiều máy/VM QNX qua mạng thật.
- **Cấu hình TRAFFIC_NODE_MAP**: baseline (mục 2).
- **Chuẩn bị**: dừng sạch mọi tiến trình từ test trước.
- **Các bước**:
  1. Khởi động `./rlx_main 2` (chỉ RL2) trên VM3.
  2. Khởi động `./lx_main 3`, `./lx_main 4` (L3, L4 — 2 Lx liền kề RL2) trên
     VM2.
  3. Khởi động `./c_main` trên VM1.
  4. Khởi động tiếp phần còn lại: `./rlx_main 1`, `./rlx_main 3` (VM3),
     `./lx_main 1`, `./lx_main 2`, `./lx_main 5`, `./lx_main 6` (VM2) — theo
     thứ tự xen kẽ tuỳ ý, không cần theo khối vai trò.
  5. Sau khi toàn bộ 9 tiến trình + C1 đã chạy, kiểm tra bảng trạng thái
     Central và thử một broadcast `t` (chain R1) để xác nhận toàn hệ thống
     hoạt động đầy đủ.
- **Kết quả mong đợi**:
  - Không tiến trình nào crash ở bất kỳ bước nào, bất kể thứ tự.
  - Bảng trạng thái Central cuối cùng hiển thị đủ 9/9 `AVAILABLE`, không phụ
    thuộc thứ tự khởi động cụ thể ở bước 1-4.
  - Lệnh `t` (chain R1) ở bước 5 nhận đủ 3 ACK từ L1, L3, L5 — xác nhận hệ
    thống hội tụ về trạng thái hoạt động đúng bất kể trình tự khởi động ban
    đầu, miễn là mọi node cuối cùng đều lên và `TRAFFIC_NODE_MAP` mỗi phía
    đúng.

---

## 6. Bảng tổng hợp truy vết nhanh

| TC | Luồng | Loại | Node bắt buộc phải khác VM |
|---|---|---|---|
| TC-XNODE-01 | SET_TIMING_PROFILE R1 | Positive | C1, L1, L3, L5 |
| TC-XNODE-02 | SET_TIMING_PROFILE R2 | Positive | C1, L2, L4, L6 |
| TC-XNODE-03 | SET_TIMING_PROFILE, 1 đích chưa lên | Negative | C1, L1, L3, (L5 muộn) |
| TC-XNODE-04 | REQUEST_OVERRIDE arterial | Positive | C1, L2 |
| TC-XNODE-05 | REQUEST_OVERRIDE connector | Positive | C1, L4 |
| TC-XNODE-06 | REQUEST_OVERRIDE bị NACK (railway conflict) | Negative | C1, L1, RL1 |
| TC-XNODE-07 | REQUEST_OVERRIDE lúc ped clearance | Edge case | C1, L3 |
| TC-XNODE-08 | RENEW/CANCEL_OVERRIDE | Positive | C1, L2 |
| TC-XNODE-09 | SET_MODE | Positive | C1, L6 |
| TC-XNODE-10 | REQUEST_FAULT_CLEAR | Positive | C1, RL2 |
| TC-XNODE-11 | CROSSING_STATUS tới 3 nơi (đủ) | Positive | RL1, L1, L2, C1 |
| TC-XNODE-12 | CROSSING_STATUS, 1 đích chưa lên | Negative/Edge | RL1, L1, (L2 muộn), C1 |
| TC-XNODE-13 | Kill 1 Lx, watchdog 3s | Positive | C1, tất cả Lx/RLx |
| TC-XNODE-14 | Kill 1 RLx giữa lúc WARNING | Edge case | C1, L3, L4, RL2 |
| TC-XNODE-15 | Restart node đã tắt | Positive | C1, L4 |
| TC-XNODE-16 | TRAFFIC_NODE_MAP sai tên node | Negative | C1, L4 (thật) |
| TC-XNODE-17 | TRAFFIC_NODE_MAP thiếu entry | Negative | C1, L4 (thật) |
| TC-XNODE-18 | Khởi động ngược thứ tự | Positive | toàn bộ 10 node |
| TC-XNODE-19 | Khởi động thứ tự xen kẽ khác | Positive | toàn bộ 10 node |
