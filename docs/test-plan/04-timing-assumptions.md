# 04. Test Plan — Xác minh các giả định về thời gian (Timing Assumptions)

Tài liệu này kiểm thử xem các **giá trị số/thời gian** công bố trong
`system_assumptions_tables.md` (TC-01..05, TL-01..06, DP-01/02, CC-01/03,
RC-03/04/06, PA-07, PA-11/12) có thực sự khớp với hành vi runtime của hệ
thống khi chạy trên QNX Neutrino thật hay không. Các assumption thuần vật
lý/quy hoạch không thể quan sát bằng cách chạy chương trình (NU-01, RC-08,
TC-01's khoảng cách 350/400/320/380 m, RC-08's tốc độ tàu 80 km/h, v.v.)
**không** nằm trong phạm vi tài liệu này.

Mọi con số dưới đây được đối chiếu trực tiếp với mã nguồn tại thời điểm viết
tài liệu (không suy đoán) — xem cột "Nguồn code" trong bảng 0.3.

---

## 0. Quy ước chung

### 0.1 Ba môi trường thử nghiệm

| Ký hiệu | Mô tả | Khi nào dùng |
| --- | --- | --- |
| **(A)** | 1 node đơn: chỉ một tiến trình (`lx_main`, `rlx_main`, hoặc `c_main`) chạy trên một target/VM QNX. Không cần `TRAFFIC_NODE_MAP`. | Test hằng số thời gian nội tại của một FSM (TL-xx, RC-xx phần đếm giờ nội bộ, PA-11/12 cục bộ). |
| **(B)** | Nhiều node cùng một máy/VM QNX (nhiều tiến trình `lx_main`/`rlx_main`/`c_main` chạy trên cùng một target, giao tiếp qua Qnet loopback nội bộ, không cần khai `TRAFFIC_NODE_MAP` vì mặc định "same node"). | Test tương tác nhiều controller (PA-07 watchdog, RC-xx với cả `Lx` lẫn `RLx`, PA-11/12 round-trip C1↔Lx) khi không đủ 2-3 máy vật lý. |
| **(C)** | Nhiều VM/máy QNX thật qua mạng Qnet thật (bắt buộc khai `TRAFFIC_NODE_MAP`, xem `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` mục 2.2–2.3 và Case 1/2/3). | **Bắt buộc cho TC-01..05** vì green-wave offset chỉ có ý nghĩa kiểm thử thật khi `L1/L3/L5` (hoặc `L2/L4/L6`) là các tiến trình độc lập, khởi động lệch giờ nhau, đồng bộ **duy nhất** qua đồng hồ tường (wall clock) — đúng như thuật toán `lx_fsm_apply_offset_locked()` giả định. Có thể hạ cấp xuống (B) nếu chấp nhận chạy cùng máy (xem ghi chú "giới hạn" ở mỗi test case TC-TIME). |

Ví dụ khai báo cho (C), lấy từ `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`:
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
```

### 0.2 Dung sai đo lường — vì sao không đòi hỏi chính xác tới mili giây

- **Tick pha đèn/railway cố định 100 ms** (`LX_PHASE_TICK_MS`, `lx_timer.h:40`) cho `Lx`, và tick 1000 ms cho `RLx`/heartbeat/watchdog. Mọi ngưỡng thời gian chỉ được cập nhật tại các bội số của tick đó — không có ý nghĩa đòi hỏi độ chính xác dưới 100 ms (Lx) hoặc dưới 1 s (RLx/PA-07).
- **Không có đồng hồ monotonic dùng chung giữa các node.** `lx_fsm_apply_offset_locked()` (`app/intersection/src/lx_fsm.c:564-614`) đồng bộ offset xanh sóng bằng `clock_gettime(CLOCK_REALTIME, ...)` — tức là **đồng hồ tường** của từng máy/VM. Nếu đồng hồ hệ thống của các VM lệch nhau (không có NTP/đồng bộ thời gian), sai số đó sẽ cộng thẳng vào sai số offset đo được. **Trước khi chạy bất kỳ test TC-TIME nào, phải đồng bộ đồng hồ tường của tất cả VM tham gia** (ví dụ `date` thủ công hoặc NTP nếu có mạng ra ngoài) và ghi nhận độ lệch còn lại (nên < 200 ms).
- **Log của `C1` (`c_logger.c`) chỉ có độ phân giải 1 giây** (`strftime("%Y-%m-%d %H:%M:%S", ...)`, `app/central/src/c_logger.c:29-31`) — không có mili giây. Dùng log này để đo khoảng cách hai sự kiện cách nhau vài giây là chấp nhận được (sai số ±1 s do làm tròn giây), nhưng **không** dùng để đo các khoảng < 2 s.
- **Log của `Lx`/`RLx` (`lx_signal.c`, `rlx_signal.c`, `rlx_gate.c`) hoàn toàn KHÔNG có timestamp** — chỉ là `printf` thuần (ví dụ `"Lx %d: SIGNAL -> %s\n"`, `app/intersection/src/lx_signal.c:42`). Do đó, mọi test case cần đo thời gian giữa hai dòng log của `Lx`/`RLx` **phải** dùng một trong hai cách:
  1. **Đồng hồ bấm giờ thủ công** (điện thoại): bắt đầu bấm ngay khi dòng log xuất hiện trên màn hình console SSH, dừng khi dòng log tiếp theo xuất hiện. Cách này có sai số người dùng ước tính **±300–500 ms** (thời gian phản xạ bấm nút) — cộng thêm vào dung sai hệ thống.
  2. Chuyển hướng stdout qua một wrapper gắn timestamp trước khi ghi ra file/console, ví dụ (nếu shell trên target hỗ trợ): `/tmp/lx_main 1 | while IFS= read -r line; do echo "$(date +%T.%3N) $line"; done > /tmp/l1.log`. Cách này chính xác hơn stopwatch nhưng phụ thuộc `date` có hỗ trợ mili giây (`%3N`) trên QNX shell đang dùng — kiểm tra trước, nếu không hỗ trợ thì dùng cách 1.
- **Dung sai đề xuất mặc định cho mọi test đo bằng stopwatch tay:** giá trị kỳ vọng ± (500 ms + 1 tick liên quan). Test case cụ thể bên dưới ghi rõ con số áp dụng.

### 0.3 Bảng hằng số thời gian — đối chiếu trực tiếp với code (nguồn sự thật)

| Hằng số | Giá trị | File:dòng | Khớp assumption |
| --- | --- | --- | --- |
| `LX_PEAK_ARTERIAL_GREEN_MS` | 48000 | `lx_timer.h:23` | TL-02 |
| `LX_PEAK_CONNECTOR_GREEN_MS` | 30000 | `lx_timer.h:24` | TL-02 |
| `LX_YELLOW_MS` | 4000 | `lx_timer.h:27` | TL-01 |
| `LX_ALL_RED_MS` | 2000 | `lx_timer.h:28` | TL-01 |
| `LX_MIN_GREEN_MS` | 8000 | `lx_timer.h:31` | TL-01 |
| `LX_MAX_GREEN_MS` | 40000 | `lx_timer.h:32` | TL-01 |
| `LX_EXTENSION_MS` | 4000 | `lx_timer.h:33` | TL-03/CC-03 |
| `LX_OVERRIDE_DURATION_CAP_MS` | 300000 | `lx_timer.h:36` | PA-11 |
| `LX_PHASE_TICK_MS` | 100 | `lx_timer.h:40` | (tick hệ thống) |
| `LX_WALK_MS` | 6000 | `lx_timer.h:57` | TL-05 (placeholder, không phải giá trị spec) |
| `LX_FLASHING_DONT_WALK_MS` | 4000 | `lx_timer.h:58` | TL-05 (placeholder) |
| `LX_DRAIN_MAX_EXTENSION_MS` | 60000 | `lx_timer.h:71` | CC-03 |
| `LX_CYCLE_LENGTH_MS` | 90000 (= 48+4+2+30+4+2 s, tính từ các hằng số trên) | `lx_timer.h:81-82` | TL-02/TC-02 |
| `R1_L1_OFFSET_MS` / `R1_L3_OFFSET_MS` / `R1_L5_OFFSET_MS` | 0 / 21000 / 45000 | `c_mode_eng.h:53-55` | TC-02 |
| `R2_L2_OFFSET_MS` / `R2_L4_OFFSET_MS` / `R2_L6_OFFSET_MS` | 0 / 19000 / 42000 | `c_mode_eng.h:56-58` | TC-02 |
| `C_MODE_ENG_DEFAULT_PEAK_START_HOUR` / `_END_HOUR` | 6 / 9 | `c_mode_eng.h:49-50` | DP-02 (placeholder tùy chỉnh) |
| Ngưỡng NACK offset | `offset_ms >= LX_CYCLE_LENGTH_MS` (90000) → NACK | `lx_fsm.c:625-636` | PA-09 |
| `RLX_WARNING_TO_CLOSING_MS` | 5000 | `rlx_timer.h:15` | RC-03 |
| `RLX_CLOSING_DEADLINE_MS` | 15000 (tính **từ lúc vào state CLOSING**, tức là **5000+15000=20000 ms kể từ TRAIN_APPROACHING** mới raise fault — khớp mốc "20 s" của Appendix B4, không phải 15 s) | `rlx_timer.h:16`; áp dụng tại `rlx_fsm.c:139` (`enter_closing()` reset `state_elapsed_ms=0` tại `rlx_fsm.c:162-167`) | RC-03 |
| `RLX_OCCUPANCY_WINDOW_MS` | 20000 | `rlx_timer.h:21` | RC-04 |
| `RLX_OPENING_DEADLINE_MS` | 15000 (từ lúc vào state OPENING) | `rlx_timer.h:22` | RC-06 (giá trị nội bộ, không có số tương ứng trực tiếp trong bảng chính) |
| `RLX_GATE_MOTION_MS` | 3000 (thời gian mô phỏng chắn di chuyển — dùng để biết đường-đi bình thường sẽ xác nhận CLOSED/OPEN nhanh hơn nhiều so với deadline) | `rlx_gate.h:10` | (hỗ trợ RC-03/RC-06) |
| Heartbeat period | 1000 ms cả hai chiều (`Lx`/`RLx` gửi, `C1` tick đếm) | `lx_main.c:179`, `c_main.c:174` | PA-07 |
| Ngưỡng UNAVAILABLE | `missed_heartbeat_ticks == 3` (tick 1 Hz) → khoảng **2.0–3.0 s** kể từ heartbeat cuối cùng, không phải đúng 3.000 s (xem TC PA-TIME-02) | `c_watchdog_mon.c:4-16` | PA-07 |
| Cap thời lượng override | `duration_ms == 0 \|\| duration_ms > 300000` → NACK (cả ở `Lx` và ở lớp validate của `C1`) | `lx_fsm.c:701`, `c_mode_eng.c:113` | PA-11 |

**Ghi chú quan trọng phát hiện khi đọc code (ảnh hưởng cách viết test DP-01/02):**
`c_mode_eng_select_mode()` (`app/central/src/c_mode_eng.c:56-62`) nhận `current_hour`
làm tham số do **caller truyền vào** — bản thân hàm này không đọc đồng hồ hệ
thống. Rà toàn bộ `app/central/src/c_main.c` và `app/central/src/c_operator.c`
xác nhận **không có nơi nào trong luồng chạy thực tế gọi hàm này với giờ
đồng hồ tường thật**; phía `Lx`/`RLx` cũng không có logic đọc `tm_hour`/
`localtime` nào tương tự. Nói cách khác: **việc tự động chuyển
`PEAK_FIXED` ↔ `OFF_PEAK_SENSOR` theo giờ đồng hồ (DP-02) hiện KHÔNG được
nối dây (wired) vào hệ thống đang chạy** — đây là một khoảng trống hiện
thực, không phải giả định sai. Các test case DP-TIME-xx bên dưới phản ánh
đúng thực trạng này thay vì giả vờ tính năng đã tồn tại.

---

## 1. TC-01..05 — Offset xanh sóng (Green-wave)

> Môi trường bắt buộc: **(C)** cho phép đo thật giữa các node độc lập; có
> thể hạ xuống (B) nếu chấp nhận múi giờ hệ thống giống hệt nhau (cùng máy)
> — trong trường hợp đó ghi rõ trong biên bản test là "chạy trên (B), chưa
> kiểm chứng đồng bộ đồng hồ liên-VM thật".

### TC-TIME-01: Offset R1 — L1 → L3 = 21 s
- **Loại**: Positive
- **Liên quan**: TC-02, `R1_L3_OFFSET_MS = 21000` (`c_mode_eng.h:54`)
- **Môi trường**: (C) — `L1`, `L3`, `L5` chạy trên 3 VM khác nhau (hoặc tối thiểu `L1`/`L3` trên 2 VM khác nhau), `C1` trên VM riêng, đã đồng bộ đồng hồ tường giữa các VM (chênh lệch đo trước < 200 ms bằng `date` trên từng VM).
- **Chuẩn bị**: Khởi động `c_main`, `rlx_main 1..3`, `lx_main 1..6` theo đúng thứ tự ở `QNX_DEPLOYMENT_RUN_GUIDE.md` mục 2.3. Để hệ thống chạy ổn định ở `MODE_PEAK_FIXED` (mode mặc định khi khởi động, `lx_fsm.c:360`) ít nhất 1 chu kỳ 90 s trước khi bắt đầu đo, để `assigned_offset_ms` (gửi từ `C1` qua `SET_TIMING_PROFILE`, UC-03) đã được áp dụng vào một pha ARTERIAL_GREEN mới (xem TC-TIME-04 về độ trễ áp dụng).
- **Các bước**:
  1. Trên console `L1`, chờ dòng log `"Lx 1: SIGNAL -> ARTERIAL GREEN"` xuất hiện.
  2. Ngay khi thấy dòng log đó, bấm Start trên đồng hồ bấm giờ điện thoại.
  3. Chuyển sang/quan sát song song console `L3`, chờ dòng log `"Lx 3: SIGNAL -> ARTERIAL GREEN"` của **chu kỳ ngay sau đó** (không phải một chu kỳ ngẫu nhiên khác).
  4. Bấm Stop ngay khi dòng log đó xuất hiện.
- **Kết quả mong đợi**: Thời gian đo được nằm trong khoảng **20.5–21.5 s** (21 s ± 500 ms do tick 100 ms + độ trễ phản xạ người bấm + sai số đồng bộ đồng hồ liên-VM).

### TC-TIME-02: Offset R1 — L3 → L5 = 24 s (cộng dồn L1 → L5 = 45 s)
- **Loại**: Positive
- **Liên quan**: TC-02, `R1_L5_OFFSET_MS = 45000` (`c_mode_eng.h:55`); đoạn `L3→L5` = 45000-21000 = 24000 ms.
- **Môi trường**: (C), tiếp nối trạng thái đã ổn định từ TC-TIME-01.
- **Chuẩn bị**: Như TC-TIME-01.
- **Các bước**: Lặp lại thao tác TC-TIME-01 nhưng bấm Start tại `"Lx 3: SIGNAL -> ARTERIAL GREEN"` và Stop tại `"Lx 5: SIGNAL -> ARTERIAL GREEN"` của chu kỳ kế tiếp. Đồng thời đo lại trực tiếp L1 → L5 (bắc cầu 2 chu kỳ chờ nếu cần) để đối chiếu cộng dồn.
- **Kết quả mong đợi**: L3 → L5 nằm trong **23.5–24.5 s**; L1 → L5 (đo trực tiếp hoặc suy ra) nằm trong **44.5–45.5 s**.

### TC-TIME-03: Offset R2 — L2 → L4 = 19 s, L4 → L6 = 23 s (cộng dồn 42 s)
- **Loại**: Positive
- **Liên quan**: TC-02, `R2_L4_OFFSET_MS = 19000`, `R2_L6_OFFSET_MS = 42000` (`c_mode_eng.h:57-58`)
- **Môi trường**: (C)
- **Chuẩn bị**: Như TC-TIME-01, áp dụng cho chuỗi R2 (`L2`→`L4`→`L6`).
- **Các bước**: Đo `"Lx 2: SIGNAL -> ARTERIAL GREEN"` → `"Lx 4: SIGNAL -> ARTERIAL GREEN"` (kỳ vọng ~19 s) và `"Lx 4: ..."` → `"Lx 6: ..."` (kỳ vọng ~23 s), cùng phương pháp stopwatch như trên.
- **Kết quả mong đợi**: L2→L4 trong **18.5–19.5 s**; L4→L6 trong **22.5–23.5 s**.

### TC-TIME-04: Edge case — offset chỉ áp dụng tại lần vào ARTERIAL_GREEN mới, không cắt ngang pha đang chạy
- **Loại**: Edge case
- **Liên quan**: TC-03, thuật toán `lx_fsm_apply_offset_locked()` (`lx_fsm.c:481-563`, đặc biệt đoạn "Compliance-audit fix" — offset chỉ set cờ `offset_apply_pending` tại `lx_fsm_on_set_timing_profile()`, `lx_fsm.c:645`, và chỉ thực sự áp tại `lx_fsm.c:344` khi một `PHASE_ARTERIAL_GREEN` **mới** bắt đầu).
- **Môi trường**: (B) hoặc (C) — chỉ cần 1 `L1` + `C1`.
- **Chuẩn bị**: Cho `L1` chạy ở `PEAK_FIXED`. Dùng công cụ gửi lệnh operator (`C1` phím `t`) để gửi `SET_TIMING_PROFILE` với `offset_ms` mới **ngay giữa lúc `L1` đang hiển thị ARTERIAL GREEN** (quan sát console `L1`, gửi lệnh khi biết chắc còn > 20 s green còn lại).
- **Các bước**:
  1. Ghi lại thời điểm gửi lệnh (theo đồng hồ bấm giờ) và thời điểm `"Lx 1: SIGNAL -> ARTERIAL YELLOW"` xuất hiện ngay sau đó (kết thúc pha ARTERIAL_GREEN hiện tại).
  2. Đo khoảng thời gian từ lúc gửi lệnh đến khi pha ARTERIAL_GREEN đó **thực sự kết thúc** (chuyển sang YELLOW).
  3. Đo khoảng thời gian pha ARTERIAL_GREEN đó tồn tại **từ lúc bắt đầu pha** (không phải từ lúc gửi lệnh) đến lúc chuyển YELLOW.
- **Kết quả mong đợi**: Pha ARTERIAL_GREEN **đang chạy tại thời điểm gửi lệnh** phải có tổng thời lượng đúng **48 s ± 200 ms** (không bị rút ngắn/kéo dài bởi offset mới) — tức lệnh `SET_TIMING_PROFILE` không cắt ngang pha hiện tại. Offset mới chỉ được quan sát ở lần ARTERIAL_GREEN **kế tiếp** trở đi (đối chiếu bằng TC-TIME-01/02/03 lặp lại sau khi gửi lệnh).

### TC-TIME-05: Negative — offset_ms = LX_CYCLE_LENGTH_MS (90000) bị NACK; 89999 vẫn ACK
- **Loại**: Negative + Edge case (ranh giới)
- **Liên quan**: PA-09, kiểm tra `payload->offset_ms >= LX_CYCLE_LENGTH_MS` tại `lx_fsm.c:625-636` (trả `RESULT_NACK` / `NACK_REASON_STALE_OR_UNSAFE_PROFILE`)
- **Môi trường**: (A) hoặc (B) — chỉ cần 1 `L1` nhận lệnh trực tiếp (có thể qua `C1` operator console hoặc một client test gửi `MSG_SET_TIMING_PROFILE` thủ công).
- **Chuẩn bị**: `L1` đang chạy bình thường, không có fault.
- **Các bước**:
  1. Gửi `SET_TIMING_PROFILE` với `offset_ms = 89999` → quan sát log `C1`: `"C1: SET_TIMING_PROFILE to 1 -> ACK"`.
  2. Gửi `SET_TIMING_PROFILE` với `offset_ms = 90000` (đúng bằng `LX_CYCLE_LENGTH_MS`) → quan sát log `C1`.
  3. (Tuỳ chọn) Gửi thêm `offset_ms = 90001` để xác nhận cùng hành vi NACK.
- **Kết quả mong đợi**: Bước 1 → `ACK`. Bước 2 và 3 → `"C1: SET_TIMING_PROFILE to 1 -> NACK reason=STALE_OR_UNSAFE_PROFILE"` (tên in ra bởi `nack_reason_name()`, `c_comm.c:57`). Đây là ranh giới đúng-tại-N-1/N: 89999 hợp lệ, 90000 (N) bị từ chối.

---

## 2. TL-01..06 — Thời gian pha đèn giao thông

### TL-TIME-01: Positive — min green 8 s được tôn trọng khi không có nhu cầu
- **Loại**: Positive
- **Liên quan**: TL-01, `LX_MIN_GREEN_MS = 8000` (`lx_timer.h:31`), enforced tại `lx_timer_should_exit_green()` (`lx_timer.c:19-30`: `if (elapsed_ms < LX_MIN_GREEN_MS) return 0`)
- **Môi trường**: (A) — 1 `L1` độc lập.
- **Chuẩn bị**: Chuyển `L1` sang `MODE_OFF_PEAK_SENSOR` (lệnh `SET_MODE` từ `C1`, hoặc chờ nếu mode mặc định khác — xác nhận mode hiện tại qua log/HMI trước khi test). Đảm bảo không có demand nào (arterial/connector) trước khi ARTERIAL_GREEN bắt đầu — dùng phím sensor để xoá demand (`C`/`A` viết hoa = clear).
- **Các bước**: Bấm Start ngay khi `"Lx 1: SIGNAL -> ARTERIAL GREEN"` xuất hiện, không tạo bất kỳ demand nào trong suốt phiên. Bấm Stop khi `"Lx 1: SIGNAL -> ARTERIAL YELLOW"` xuất hiện.
- **Kết quả mong đợi**: Thời gian đo được **≥ 8.0 s** (không được kết thúc sớm hơn), và trong khoảng **8.0–8.5 s** vì extension check chỉ chạy tại bội số 4000 ms và 8000 ms là điểm kiểm tra thoát-pha đầu tiên khi không có demand (`lx_fsm.c:981` `if ((green_elapsed_ms % LX_EXTENSION_MS) == 0)`).

### TL-TIME-02: Edge case — ranh giới đúng 7.9 s vs 8.0 s
- **Loại**: Edge case
- **Liên quan**: TL-01, cùng cơ chế TL-TIME-01, ranh giới `elapsed_ms < 8000` (giữ) vs `elapsed_ms == 8000` (được phép thoát).
- **Môi trường**: (A)
- **Chuẩn bị**: Như TL-TIME-01, không demand.
- **Các bước**: Đo chính xác thời điểm pha ARTERIAL_GREEN bắt đầu và theo dõi liên tục đến khi chuyển YELLOW. Vì tick hệ thống là 100 ms, "N-1/N" thực tế cần kiểm chứng là **7.9 s (chưa được thoát) và 8.0 s (điểm kiểm tra thoát đầu tiên hợp lệ)**, không phải mili giây lẻ (hệ thống không có độ phân giải dưới 100 ms).
- **Kết quả mong đợi**: Tuyệt đối không quan sát thấy `ARTERIAL YELLOW` trước mốc 7.9 s (dung sai đo −0/+300 ms do stopwatch tay); pha phải kết thúc trong cửa sổ 8.0–8.5 s như TL-TIME-01 nếu không có demand.

### TL-TIME-03: Positive — max green 40 s ép thoát pha dù còn demand
- **Loại**: Positive
- **Liên quan**: TL-01, `LX_MAX_GREEN_MS = 40000` (`lx_timer.h:32`), `maxed = elapsed_ms >= LX_MAX_GREEN_MS` (`lx_timer.c:27-29`)
- **Môi trường**: (A)
- **Chuẩn bị**: `MODE_OFF_PEAK_SENSOR`. Tạo demand arterial liên tục (phím `a` giữ "present" và không bao giờ nhấn `A` để clear) trong suốt pha.
- **Các bước**: Bấm Start tại `ARTERIAL GREEN`, giữ demand liên tục, Stop tại `ARTERIAL YELLOW`.
- **Kết quả mong đợi**: Pha kết thúc trong khoảng **40.0–40.5 s** dù demand vẫn còn (chứng minh cap 40 s thắng demand liên tục), không được vượt quá 40.5 s.

### TL-TIME-04: Edge case — ranh giới 39.9 s (chưa maxed, còn demand thì giữ) vs 40.0 s (maxed, buộc thoát)
- **Loại**: Edge case
- **Liên quan**: TL-01, cùng cơ chế TL-TIME-03.
- **Môi trường**: (A)
- **Chuẩn bị**: Như TL-TIME-03.
- **Các bước**: Theo dõi liên tục quanh mốc 39.9–40.1 s (đo bằng stopwatch, chấp nhận sai số ±300 ms vì đây là quan sát bằng mắt qua console).
- **Kết quả mong đợi**: Tại ~39.9 s pha vẫn là ARTERIAL GREEN (vì `elapsed_ms < 40000`, demand còn nên `should_exit` trả 0 nếu chưa maxed); pha phải chuyển YELLOW trong cửa sổ 40.0–40.5 s bất kể demand.

### TL-TIME-05: Positive — yellow đúng 4 s
- **Loại**: Positive
- **Liên quan**: TL-01, `LX_YELLOW_MS = 4000` (`lx_timer.h:27`), kiểm tra `green_elapsed_ms >= LX_YELLOW_MS` tại `lx_fsm.c:944`
- **Môi trường**: (A), áp dụng cho cả `PEAK_FIXED` lẫn `OFF_PEAK_SENSOR` (hằng số dùng chung).
- **Chuẩn bị**: Bất kỳ mode nào, chờ một pha YELLOW xuất hiện.
- **Các bước**: Bấm Start tại `"SIGNAL -> ARTERIAL YELLOW"` (hoặc CONNECTOR YELLOW), Stop tại `"SIGNAL -> ALL RED (...)"` kế tiếp.
- **Kết quả mong đợi**: **3.9–4.1 s** (4 s ± 100 ms tick + ±300 ms sai số bấm tay → làm tròn dung sai chấp nhận **3.6–4.4 s**).

### TL-TIME-06: Positive — all-red clearance đúng 2 s
- **Loại**: Positive
- **Liên quan**: TL-01, `LX_ALL_RED_MS = 2000` (`lx_timer.h:28`), kiểm tra tại `lx_fsm.c:951`
- **Môi trường**: (A)
- **Chuẩn bị**: Như trên.
- **Các bước**: Bấm Start tại `"SIGNAL -> ALL RED (A to B)"`, Stop tại `"SIGNAL -> CONNECTOR GREEN"` (hoặc pha kế tiếp tương ứng).
- **Kết quả mong đợi**: **1.6–2.4 s** (2 s ± 100 ms tick + ±300 ms sai số tay).

### TL-TIME-07: Positive — chuỗi người đi bộ WALK (6 s) → FLASHING_DONT_WALK (4 s), tổng 10 s
- **Loại**: Positive
- **Liên quan**: TL-05/TL-06, `LX_WALK_MS = 6000`, `LX_FLASHING_DONT_WALK_MS = 4000` (`lx_timer.h:57-58` — **lưu ý đây là giá trị placeholder tự chọn của nhóm, không phải số bắt buộc từ đề bài**, nhưng vẫn phải khớp đúng con số đã code).
- **Môi trường**: (A)
- **Chuẩn bị**: Ở một pha tương thích (ví dụ ARTERIAL_GREEN), nhấn nút bộ hành (phím `1`) để tạo `PED_REQUEST` phía tương thích.
- **Các bước**: Bấm Start tại `"PED SIGNAL side 0 -> WALK"`, lap thời gian tại `"PED SIGNAL side 0 -> FLASHING_DONT_WALK"`, Stop tại `"PED SIGNAL side 0 -> DONT_WALK"`.
- **Kết quả mong đợi**: WALK kéo dài **5.6–6.4 s**, FLASHING_DONT_WALK kéo dài **3.6–4.4 s**, tổng chuỗi **9.6–10.4 s**.

### TL-TIME-08: Negative/toàn vẹn — tổng chu kỳ PEAK_FIXED phải bằng đúng 90 s
- **Loại**: Negative (kiểm tra không có trôi/lệch cộng dồn)
- **Liên quan**: TL-02, `LX_CYCLE_LENGTH_MS = 90000` (`lx_timer.h:81-82`, tính từ 48+4+2+30+4+2)
- **Môi trường**: (A)
- **Chuẩn bị**: `MODE_PEAK_FIXED`, không có override/railway pre-emption/fault xảy ra trong lúc đo (mọi can thiệp làm hỏng phép đo này).
- **Các bước**: Bấm Start tại một lần `"SIGNAL -> ARTERIAL GREEN"` bất kỳ, Stop tại lần `"SIGNAL -> ARTERIAL GREEN"` **kế tiếp** (đúng 1 chu kỳ đầy đủ: arterial green+yellow+all-red+connector green+yellow+all-red).
- **Kết quả mong đợi**: **89.0–91.0 s** (90 s ± ~1 s, dung sai nới hơn các test trên vì đây là tổng của 6 khoảng đo tay cộng dồn sai số). Nếu lệch quá ±1 s liên tục qua nhiều chu kỳ, nghi ngờ có drift trong `lx_fsm_on_phase_timer()` cần điều tra thêm (không thuộc phạm vi tài liệu này).

---

## 3. DP-01/02 — Chế độ PEAK_FIXED / OFF_PEAK_SENSOR

> **Cảnh báo trước khi test**: như đã nêu ở mục 0.3, việc tự động chuyển
> mode theo giờ đồng hồ **chưa được nối vào runtime**. Ba test case dưới
> đây được thiết kế để phản ánh đúng thực trạng đó, không phải để "chứng
> minh" một tính năng chưa tồn tại.

### DP-TIME-01: Positive — giá trị placeholder mặc định đúng 6 và 9
- **Loại**: Positive (kiểm tra mã nguồn/hằng số, không phải hành vi runtime động)
- **Liên quan**: DP-02, `C_MODE_ENG_DEFAULT_PEAK_START_HOUR = 6`, `C_MODE_ENG_DEFAULT_PEAK_END_HOUR = 9` (`c_mode_eng.h:49-50`), khởi tạo bởi `c_mode_eng_init()`.
- **Môi trường**: (A) — chạy `c_main` đơn lẻ.
- **Chuẩn bị**: Không cần dàn dựng gì đặc biệt; đây là kiểm tra giá trị nạp vào struct lúc khởi động.
- **Các bước**: Nếu có sẵn hook debug/test đơn vị gọi `c_mode_eng_select_mode(&eng, 6)` và `c_mode_eng_select_mode(&eng, 8)` và `c_mode_eng_select_mode(&eng, 9)` sau `c_mode_eng_init()` — chạy và in kết quả. Nếu không có hook, kiểm tra bằng cách đọc lại `central_log.txt`/mã nguồn đã biên dịch để xác nhận hằng số không bị thay đổi khi build.
- **Kết quả mong đợi**: `select_mode(6)` → `MODE_PEAK_FIXED`, `select_mode(8)` → `MODE_PEAK_FIXED`, `select_mode(9)` → `MODE_OFF_PEAK_SENSOR` (biên đúng tại giờ 9, xem DP-TIME-02b).

### DP-TIME-02: Edge case / Known-gap — chuyển hệ thống qua mốc 06:00 hoặc 09:00 KHÔNG tự đổi mode
- **Loại**: Edge case (ranh giới giờ) kiêm Negative (xác nhận giới hạn hiện thực)
- **Liên quan**: DP-02; xác nhận không có lời gọi `c_mode_eng_select_mode()` nào trong `c_main.c`/`c_operator.c` dùng giờ hệ thống thật.
- **Môi trường**: (B) hoặc (C) — `C1` + ít nhất 1 `Lx`.
- **Chuẩn bị**: Đặt đồng hồ hệ thống VM chạy `C1` tới 05:59:00 (dùng lệnh `date` với quyền root trên QNX target — chỉ làm trên VM test, không phải máy dùng chung). Khởi động `c_main`.
- **Các bước**: Theo dõi log/HMI của `C1` liên tục qua các mốc 06:00:00 và 09:00:00 (chỉnh đồng hồ tăng dần hoặc chờ thật). Ghi nhận `operating_mode` báo cáo bởi từng `Lx` (qua HMI `c_hmi_render`) tại từng mốc.
- **Kết quả mong đợi (thực trạng hiện tại)**: **Mode KHÔNG tự động đổi** khi đồng hồ đi qua 06:00/09:00 — vì không có tiến trình nào chủ động đọc giờ và gọi `c_mode_eng_select_mode()`/gửi `SET_MODE`. Đây là kết quả "PASS" đối với việc xác minh đúng thực trạng code, nhưng đồng thời là một **discrepancy cần ghi vào biên bản Compliance Agent**: DP-02 mô tả "clock-time boundary... selects the applicable normal mode" như một hành vi runtime, trong khi code hiện tại chỉ cung cấp hàm thuần (pure function) chưa được gọi tự động — team cần hoặc (a) bổ sung caller đọc `localtime()->tm_hour` mỗi tick và gọi hàm này, hoặc (b) cập nhật tài liệu ghi rõ đây là API sẵn sàng cho tích hợp tương lai.

### DP-TIME-03: Positive — đường duy nhất đang hoạt động: SET_MODE thủ công qua operator console
- **Loại**: Positive
- **Liên quan**: DP-01 (2 mode tồn tại và chuyển được), qua lệnh `MSG_SET_MODE` (`lx_fsm_on_set_mode`, `lx_fsm.c:689+` - đã dịch xuống sau các fix TC-02/TC-03 phía trên trong file), operator console phím `m` (`c_operator.c`).
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: `C1` và `L1` đang chạy, `L1` ở `MODE_PEAK_FIXED` (mặc định).
- **Các bước**: Trên console `C1`, nhấn `m`, chọn `L1`, chọn mode `1` (OFF_PEAK_SENSOR). Quan sát `central_log.txt`.
- **Kết quả mong đợi**: Dòng log `"C1: SET_MODE to 1 -> ACK"` xuất hiện gần như ngay lập tức (trong vòng 1 s theo đồng hồ giây của logger — xem thêm PA-TIME-06 về round-trip); hành vi pha đèn của `L1` chuyển sang sensor-driven ngay từ ranh giới pha an toàn tiếp theo (TL-04).

---

## 4. CC-01/03 — Phát hiện hàng chờ & pha xả (drain)

### CC-TIME-01: Positive — pha xả kéo dài theo bước 4 s trong khi QUEUE_WARNING còn hiệu lực
- **Loại**: Positive
- **Liên quan**: CC-03, `LX_EXTENSION_MS = 4000` tái sử dụng cho nhịp kiểm tra drain (`lx_fsm.c:1021` `if ((drain_extension_total_ms % LX_EXTENSION_MS) == 0)`)
- **Môi trường**: (B) hoặc (C) — cần `RLx` để kích hoạt kịch bản mở lại đường ngang (drain chỉ được arm qua `drain_pending` khi crossing reopen với `queue_warning_active`, `lx_fsm.c:809-822`).
- **Chuẩn bị**: Đưa `RLx` liên quan vào trạng thái đóng chắn (mô phỏng tàu tới), đồng thời bật `QUEUE_WARNING` trên approach connector của `Lx` liên quan (phím `w`). Chờ chắn mở lại (xem mục 5 để biết timing chắn).
- **Các bước**: Sau khi pha connector-drain bắt đầu, giữ `QUEUE_WARNING` bật liên tục, đo khoảng cách giữa các lần "gia hạn" — vì không có log riêng cho mỗi lần gia hạn 4 s, đo tổng thời lượng pha connector-drain từ lúc bắt đầu đến khi bạn chủ động tắt `QUEUE_WARNING` (phím `W`) và quan sát nó kết thúc **ngay tại điểm kiểm tra 4 s tiếp theo**, không phải ngay lập tức.
- **Kết quả mong đợi**: Sau khi tắt `QUEUE_WARNING`, pha vẫn tiếp tục tối đa thêm gần 4 s trước khi chuyển YELLOW (vì điều kiện chỉ được re-check tại bội số 4000 ms của `drain_extension_total_ms`) — quan sát độ trễ tắt nằm trong khoảng **0–4.3 s** kể từ lúc tắt cờ.

### CC-TIME-02: Edge case — cap cứng tại đúng 60 s dù QUEUE_WARNING vẫn còn
- **Loại**: Edge case
- **Liên quan**: CC-03, `LX_DRAIN_MAX_EXTENSION_MS = 60000` (`lx_timer.h:71`), kiểm tra `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` (`lx_fsm.c:1022`)
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: Như CC-TIME-01, nhưng **giữ `QUEUE_WARNING` bật liên tục và không bao giờ tắt**.
- **Các bước**: Bấm Start ngay khi pha drain bắt đầu (dòng `"SIGNAL -> CONNECTOR GREEN"` ngay sau khi chắn báo mở, với `drain_pending` đã được arm trước đó). Stop khi `"SIGNAL -> CONNECTOR YELLOW"` xuất hiện.
- **Kết quả mong đợi**: Pha kết thúc trong khoảng **60.0–60.5 s** kể từ lúc bắt đầu drain (không vượt quá — cap là cứng, `>=` không phải `>`), bất kể `QUEUE_WARNING` vẫn còn active.

### CC-TIME-03: Positive — drain kết thúc sớm ngay khi QUEUE_WARNING tự nhiên hết (không chờ đủ 60 s)
- **Loại**: Positive
- **Liên quan**: CC-03 (điều kiện kết thúc "queue warning clears OR 60s cap")
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: Như CC-TIME-01, nhưng tắt `QUEUE_WARNING` sớm, ví dụ tại giây thứ 12 (giữa hai mốc 4 s: 12000 ms là bội số của 4000, chọn mốc dễ quan sát).
- **Các bước**: Tắt `QUEUE_WARNING` ở giây 12, đo thời điểm pha chuyển sang YELLOW.
- **Kết quả mong đợi**: Pha kết thúc tại **~12.0–16.0 s** (tối đa thêm một nhịp 4 s sau khi tắt do cơ chế poll tại bội số 4000ms), **không** kéo dài tới gần 60 s.

---

## 5. RC-03/04/06 — Cảnh báo & đóng/mở chắn tàu

Log liên quan (không có timestamp, dùng stopwatch — mục 0.2):
- `"RLx: flashers ON (train approaching, direction %u)"` — bắt đầu WARNING (`rlx_signal.c:6`), tương ứng T0 của `TRAIN_APPROACHING`.
- `"RLx: commanding gates DOWN (simulated motion, 3000 ms)"` — bắt đầu CLOSING (`rlx_gate.c:48`).
- `"RLx: train signal PROCEED for direction %u (gates confirmed closed)"` — xác nhận CLOSED (`rlx_signal.c:16`).
- `"RLx: all train signals -> STOP (crossing reopening)"` — bắt đầu OPENING (`rlx_signal.c:21`).
- `"RLx: flashers OFF (gates confirmed open)"` — xác nhận OPEN (`rlx_signal.c:11`).
- `"RLx: FAULT latched (fault bit 0x...) ..."` — báo fault (`rlx_signal.c:31`).

### RC-TIME-01: Positive — warning-to-closing đúng 5 s
- **Loại**: Positive
- **Liên quan**: RC-03, `RLX_WARNING_TO_CLOSING_MS = 5000` (`rlx_timer.h:15`), kiểm tra tại `rlx_fsm.c:351`
- **Môi trường**: (A) — 1 `RL1` độc lập (điều khiển tick nội bộ 1 Hz).
- **Chuẩn bị**: `RL1` ở trạng thái `RLX_OPEN` (mặc định khởi động).
- **Các bước**: Gõ phím `0` (TRAIN_APPROACHING hướng 0) trên `rlx_sensor`. Bấm Start ngay khi `"flashers ON"` xuất hiện. Bấm Stop khi `"commanding gates DOWN"` xuất hiện.
- **Kết quả mong đợi**: **4.5–5.5 s** (5 s ± 500 ms, tick 1000 ms + sai số tay).

### RC-TIME-02: Edge case (fault path) — tổng ngân sách 20 s (không phải 15 s) trước khi FAULT_GATE_CONFIRM_MISSING
- **Loại**: Edge case + Negative (kiểm tra đường lỗi)
- **Liên quan**: RC-03/RC-06; **phát hiện quan trọng khi đọc code**: `RLX_CLOSING_DEADLINE_MS = 15000` được tính **từ lúc vào state CLOSING** (`state_elapsed_ms` reset về 0 tại `enter_closing()`, `rlx_fsm.c:162-167`), KHÔNG phải từ lúc `TRAIN_APPROACHING`. Do đó tổng thời gian thực tế trước khi fault được raise là **5000 (WARNING) + 15000 (CLOSING) = 20000 ms**, khớp đúng mốc cộng dồn "20 s — closed-confirmation margin" của Appendix B4 trong `system_assumptions_tables.md`, chứ không phải 15 s như tên hằng số dễ gây hiểu lầm.
- **Môi trường**: (A)
- **Chuẩn bị**: Gõ phím `x` trên `rlx_sensor` để arm "gate motion sẽ KHÔNG bao giờ xác nhận đóng" (demo fault, `rlx_gate.c:120`, "RC-06 fault path"). Sau đó gõ `0` để bắt đầu TRAIN_APPROACHING.
- **Các bước**: Bấm Start tại `"flashers ON"`. Theo dõi liên tục, ghi lại các mốc 19.5 s (kỳ vọng: chưa fault) và 20.0–20.5 s (kỳ vọng: fault xuất hiện).
- **Kết quả mong đợi**: KHÔNG có dòng `"FAULT latched"` trước **19.5 s**; dòng `"FAULT latched (fault bit ... ) ... commanding gates DOWN"` PHẢI xuất hiện trong cửa sổ **20.0–21.0 s** kể từ lúc `flashers ON` (dung sai nới hơn vì cộng 2 tick 1000ms rời rạc + sai số tay).

### RC-TIME-03: Positive — đường bình thường xác nhận CLOSED nhanh (~8 s), có biên an toàn lớn so với deadline 20 s
- **Loại**: Positive
- **Liên quan**: RC-03/RC-06, đường thường KHÔNG arm fault demo — `RLX_GATE_MOTION_MS = 3000` (`rlx_gate.h:10`) nên gate xác nhận đóng ~3 s sau khi vào CLOSING, tức ~8 s sau `TRAIN_APPROACHING` (5 s WARNING + ~3 s motion, làm tròn lên tick 1s kế tiếp).
- **Môi trường**: (A)
- **Chuẩn bị**: KHÔNG gõ `x` (không arm fault). Gõ `0` để bắt đầu TRAIN_APPROACHING.
- **Các bước**: Bấm Start tại `"flashers ON"`, Stop tại `"train signal PROCEED for direction 0 (gates confirmed closed)"`.
- **Kết quả mong đợi**: **7.5–9.5 s** (5 s + 3 s motion, làm tròn theo tick 1000 ms của `RLx`, cộng sai số tay). Giá trị này phải nhỏ hơn nhiều so với ngưỡng fault 20 s ở RC-TIME-02, chứng minh biên an toàn của thiết kế.

### RC-TIME-04: Positive — occupancy window đúng 20 s trước khi bắt đầu mở lại
- **Loại**: Positive
- **Liên quan**: RC-04, `RLX_OCCUPANCY_WINDOW_MS = 20000` (`rlx_timer.h:21`), đếm ngược tại `rlx_fsm.c:372-390` qua `rlx_timer_tick_window()`
- **Môi trường**: (A)
- **Chuẩn bị**: Từ trạng thái CLOSED đã xác nhận (tiếp nối RC-TIME-03), chờ đủ `RLX_EXPECTED_ARRIVAL_MS` (20 s, placeholder nội bộ) để hệ thống tự chuyển sang `TRAIN_PRESENT` (`rlx_fsm.c:362-370`) — đây là bước giả lập "tàu đã tới", không thuộc phạm vi test RC-04 nhưng cần thiết để bắt đầu đếm ngược 20 s thật.
- **Các bước**: Bấm Start ngay khi log nội bộ (không có log riêng cho việc vào TRAIN_PRESENT trong bản hiện tại — dùng thời điểm ước tính = thời điểm `PROCEED` + 20 s làm mốc tham chiếu, hoặc thêm log tạm thời nếu Verifier cho phép để phục vụ test). Stop tại `"all train signals -> STOP (crossing reopening)"` (bắt đầu OPENING).
- **Kết quả mong đợi**: Khoảng cách từ lúc vào `TRAIN_PRESENT` đến `"all train signals -> STOP"` nằm trong **19.5–20.5 s**.
- **Lưu ý**: vì không có log đánh dấu thời điểm vào `TRAIN_PRESENT`, đây là test khó đo chính xác bằng console thuần túy — khuyến nghị Verifier Agent bổ sung một dòng `printf`/log tạm cho lần chạy test này, hoặc chấp nhận đo gián tiếp qua tổng thời gian `TRAIN_APPROACHING` → `OPENING` (= 5 s + ~3 s + 20 s (RLX_EXPECTED_ARRIVAL_MS) + 20 s (RLX_OCCUPANCY_WINDOW_MS) ≈ 48 s) và so khớp với công thức thay vì đo riêng đoạn 20 s.

### RC-TIME-05: Edge case — hai tàu chồng cửa sổ occupancy, chỉ mở lại khi CẢ HAI cửa sổ hết hạn
- **Loại**: Edge case
- **Liên quan**: RC-04 ("Gates remain closed until every active occupancy window... has elapsed"), `rlx_fsm.c:384-389` (`if (active_window_count == 0) enter_opening()`)
- **Môi trường**: (A)
- **Chuẩn bị**: Sau khi tàu hướng 0 đã vào `TRAIN_PRESENT` (cửa sổ đếm ngược 20 s bắt đầu), gõ phím `1` (TRAIN_APPROACHING hướng 1) ở giây thứ ~10 để đăng ký một cửa sổ thứ hai lệch pha.
- **Các bước**: Đo thời điểm mở lại (`"all train signals -> STOP"` báo hiệu bắt đầu OPENING).
- **Kết quả mong đợi**: Thời điểm mở lại phải trễ hơn so với kịch bản chỉ-một-tàu (RC-TIME-04) một khoảng tương ứng với độ lệch đăng ký cửa sổ thứ hai (~10 s trễ hơn), **không** mở lại tại mốc 20 s của cửa sổ đầu tiên — xác nhận đúng ngữ nghĩa "đợi cả hai cửa sổ", dung sai ±1 s (tick 1000 ms).

### RC-TIME-06: Positive — xác nhận OPEN nhanh (~3 s sau khi vào OPENING), nằm sâu trong deadline 15 s
- **Loại**: Positive
- **Liên quan**: RC-06 nội bộ, `RLX_OPENING_DEADLINE_MS = 15000` (`rlx_timer.h:22`), gate motion 3000 ms
- **Môi trường**: (A)
- **Chuẩn bị**: Nối tiếp RC-TIME-04/05, không arm fault demo.
- **Các bước**: Bấm Start tại `"all train signals -> STOP (crossing reopening)"`, Stop tại `"flashers OFF (gates confirmed open)"`.
- **Kết quả mong đợi**: **2.5–4.5 s** (3 s motion ± tick 1000 ms + sai số tay), an toàn dưới ngưỡng fault 15 s.

---

## 6. PA-07 — Heartbeat & watchdog

### PA-TIME-01: Positive — heartbeat đều đặn 1 Hz
- **Loại**: Positive
- **Liên quan**: PA-07, `ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, ...)` phía `Lx` (`lx_main.c:179`) và `RLx` (tương tự) gửi `MSG_HEARTBEAT` mỗi 1 Hz.
- **Môi trường**: (B) hoặc (C) — `C1` + 1 `Lx`.
- **Chuẩn bị**: Hệ thống chạy ổn định, không lỗi.
- **Các bước**: Trong 10 giây liên tiếp (đo bằng đồng hồ), đếm số lần một dòng log liên quan tới `MSG_HEARTBEAT`/STATUS từ controller đó được ghi nhận phía `C1` (nếu không có log riêng cho từng heartbeat, dùng HMI `c_hmi_render` — vốn refresh mỗi 1 Hz theo cùng pulse — để quan sát `last_seen`/giá trị reset liên tục không "đứng hình").
- **Kết quả mong đợi**: Tần suất heartbeat quan sát được là **9–11 lần trong 10 s** (1 Hz ± 10% do jitter lịch trình hệ điều hành), tương ứng chu kỳ trung bình **0.9–1.1 s**.

### PA-TIME-02: Edge case — thời điểm đúng đắn khi bị đánh dấu UNAVAILABLE (2.0–3.0 s, không phải đúng 3.000 s)
- **Loại**: Edge case
- **Liên quan**: PA-07, `c_watchdog_mon_tick()` (`c_watchdog_mon.c:4-16`): đếm `missed_heartbeat_ticks` mỗi 1 Hz tick của `C1` (không đồng bộ pha với heartbeat của `Lx`), đánh dấu unavailable khi đếm đúng **3**. Vì tick của watchdog và heartbeat cuối cùng không đồng pha, thời gian thực tế để đạt ngưỡng dao động trong khoảng **[2.0 s, 3.0 s)** kể từ heartbeat cuối cùng nhận được — không phải chính xác 3.000 s.
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: `C1` và 1 `Lx` (ví dụ `L1`) đang chạy bình thường, trao đổi heartbeat đều.
- **Các bước**: Dừng đột ngột tiến trình `lx_main` của `L1` (kill -9, mô phỏng mất kết nối hoàn toàn — không phải tắt êm để không có gói tin cuối "cố ý" nào). Ghi lại thời điểm dừng tiến trình (T0, dùng đồng hồ hệ thống nơi chạy lệnh kill hoặc bấm giờ tay). Theo dõi `central_log.txt`, tìm dòng `"Controller 1 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"` (số `1` vì `controller_id_t` khai `CTRL_C1 = 0, CTRL_L1 = 1, ...` tại `sys_types.h:18-19`, và log in ra giá trị enum thô, không phải index mảng `controllers[]`) và đọc timestamp giây của nó.
- **Kết quả mong đợi**: Khoảng cách `[timestamp dòng UNAVAILABLE] - T0` nằm trong **1.5–4.0 s** (cộng thêm dung sai vì `c_logger` chỉ có độ phân giải giây → làm tròn ±1 s so với khoảng lý thuyết 2.0–3.0 s, cộng sai số đo T0 bằng tay).

### PA-TIME-03: Negative — KHÔNG được đánh dấu UNAVAILABLE khi mới miss 2 tick
- **Loại**: Negative
- **Liên quan**: PA-07, điều kiện chính xác `missed_heartbeat_ticks == 3` (`c_watchdog_mon.c:11`) — nghĩa là tại tick thứ 2 (~1.0–2.0 s sau heartbeat cuối) tuyệt đối chưa được đánh dấu.
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: Như PA-TIME-02, nhưng lần này **khởi động lại `L1` sớm** (gửi lại heartbeat) ngay tại khoảng 1.5 s sau khi dừng — tức trước khi tick thứ 3 kịp xảy ra.
- **Các bước**: Kill `lx_main` tại T0, khởi động lại (hoặc mô phỏng gửi 1 heartbeat thủ công) tại T0+1.5s. Kiểm tra `central_log.txt` trong toàn bộ khoảng T0 → T0+3s.
- **Kết quả mong đợi**: KHÔNG xuất hiện dòng `"marked UNAVAILABLE"` nào trong log ở khoảng thời gian này (vì `missed_heartbeat_ticks` được reset về 0 ngay khi `c_server_record_status()`/heartbeat mới tới, `c_server.c:11-12`, trước khi đạt tới 3).

---

## 7. PA-11/12 — Override cap & ACK round-trip

### PA-TIME-04: Negative + Edge case — cap 300000 ms là ranh giới ACK/NACK đúng
- **Loại**: Negative (giá trị vượt cap) + Edge case (ranh giới N/N-1)
- **Liên quan**: PA-11, `LX_OVERRIDE_DURATION_CAP_MS = 300000` (`lx_timer.h:36`), kiểm tra `duration_ms == 0 || duration_ms > LX_OVERRIDE_DURATION_CAP_MS` tại `lx_fsm.c:701` (và lớp validate sơ bộ phía `C1`, `c_mode_eng.c:113`, cùng ngưỡng 300000).
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: `L1` ở `NORMAL_OPERATION`, không có railway pre-emption/fault/ped-clearance đang chạy (để tránh rơi vào nhánh `NACK` khác hoặc `ACK_PENDING` gây nhiễu kết quả).
- **Các bước**:
  1. Gửi `REQUEST_OVERRIDE` tới `L1` với `duration_ms = 300000` (đúng bằng cap).
  2. Gửi `REQUEST_OVERRIDE` (sau khi hủy override trước bằng `CANCEL_OVERRIDE`) với `duration_ms = 300001`.
- **Kết quả mong đợi**: Bước 1 → `"C1: REQUEST_OVERRIDE to 1 -> ACK"`. Bước 2 → `"C1: REQUEST_OVERRIDE to 1 -> NACK reason=INVALID_DURATION"`. Đúng bằng cap là hợp lệ (biên `> cap` mới bị từ chối, không phải `>=`).

### PA-TIME-05: Edge case — duration_ms = 0 cũng bị NACK (không chỉ có biên trên)
- **Loại**: Edge case
- **Liên quan**: PA-11, cùng điều kiện `duration_ms == 0` tại `lx_fsm.c:701`
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: Như trên, không có override đang active.
- **Các bước**: Gửi `REQUEST_OVERRIDE` với `duration_ms = 0`.
- **Kết quả mong đợi**: `"C1: REQUEST_OVERRIDE to 1 -> NACK reason=INVALID_DURATION"` — xác nhận cap PA-11 là khoảng **(0, 300000]**, không chấp nhận 0 dù về lý thuyết "0 ≤ cap".

### PA-TIME-06: Positive — ACK trả về trong vòng 1 round-trip, thực tế dưới 1 s rất nhiều
- **Loại**: Positive
- **Liên quan**: PA-12, cơ chế `MsgSend`/`MsgReply` đồng bộ — độ trễ chỉ bị chặn bởi lịch trình OS + độ trễ mạng Qnet, không có logic trì hoãn nhân tạo nào trong `lx_fsm_on_request_override()` cho nhánh ACK tức thời (`lx_fsm.c:729-736`).
- **Môi trường**: (C) — quan trọng để đo round-trip qua mạng thật thay vì loopback nội bộ (B) vốn gần như 0 ms và không thể hiện đúng độ trễ thực tế của Qnet giữa các VM.
- **Chuẩn bị**: `L1` sẵn sàng nhận lệnh, không có điều kiện gây `ACK_PENDING`.
- **Các bước**: Trên console `C1`, gõ lệnh `o` (REQUEST_OVERRIDE) và bấm Enter — bấm Start đồng hồ ngay khi gõ Enter. Bấm Stop ngay khi dòng `"C1: REQUEST_OVERRIDE to 1 -> ACK"` xuất hiện trên console.
- **Kết quả mong đợi**: Độ trễ đo được **< 1.0 s** — trên thực tế với LAN nội bộ dự kiến chỉ **vài chục đến vài trăm mili giây**; vì `c_logger` chỉ in giây, quan sát bằng mắt cả hai sự kiện thường rơi vào **cùng một giây hiển thị** trên console, đủ để kết luận đạt yêu cầu PA-12 (không cần độ chính xác dưới giây cho test này — chỉ cần xác nhận không có độ trễ "nhìn thấy được" hàng giây).

### PA-TIME-07: Edge case — ACK_PENDING vẫn trả lời tức thời dù kích hoạt (activation) bị hoãn
- **Loại**: Edge case
- **Liên quan**: PA-12 ("only activation is deferred"), nhánh `ped_clearance_active` tại `lx_fsm.c:713-728` trả `RESULT_ACK_PENDING` ngay lập tức, còn kích hoạt thật sự chờ tới khi `lx_fsm_on_phase_timer()` phát hiện `ped_clearance_active` đã tắt.
- **Môi trường**: (B) hoặc (C)
- **Chuẩn bị**: Kích hoạt một yêu cầu bộ hành (WALK+FDW đang chạy, tổng ~10 s theo TL-TIME-07) trên `L1`, sau đó **ngay lập tức trong lúc WALK/FDW đang chạy**, gửi `REQUEST_OVERRIDE`.
- **Các bước**: Bấm Start khi gửi lệnh, Stop khi thấy `"C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING"` — đây là phản hồi tức thời cần đo (< 1 s). Sau đó, tiếp tục quan sát và đo riêng khoảng thời gian từ lúc gửi lệnh đến khi override **thực sự có hiệu lực** (ví dụ quan sát output tín hiệu chuyển theo `override_target_movement`, hoặc dòng log tương ứng nếu có) — khoảng này được PHÉP dài tới hết chuỗi ped clearance còn lại (tối đa ~10 s theo TL-05/06), không vi phạm PA-12.
- **Kết quả mong đợi**: Phản hồi `ACK_PENDING` xuất hiện **trong vòng 1 s** (thực tế gần như tức thời, tương tự PA-TIME-06); việc kích hoạt thật sự có thể trễ tới vài giây sau đó (đúng theo thiết kế) và **không** được tính là vi phạm ngưỡng phản hồi 1 s của PA-12 vì đây là hai mốc khác nhau (ACK vs. activation).

---

## Phụ lục: Tổng hợp danh sách test case

| ID | Nhóm | Loại | Môi trường |
| --- | --- | --- | --- |
| TC-TIME-01 | TC-02 | Positive | C |
| TC-TIME-02 | TC-02 | Positive | C |
| TC-TIME-03 | TC-02 | Positive | C |
| TC-TIME-04 | TC-03 | Edge case | B/C |
| TC-TIME-05 | PA-09 | Negative + Edge | A/B |
| TL-TIME-01 | TL-01 | Positive | A |
| TL-TIME-02 | TL-01 | Edge case | A |
| TL-TIME-03 | TL-01 | Positive | A |
| TL-TIME-04 | TL-01 | Edge case | A |
| TL-TIME-05 | TL-01 | Positive | A |
| TL-TIME-06 | TL-01 | Positive | A |
| TL-TIME-07 | TL-05/06 | Positive | A |
| TL-TIME-08 | TL-02 | Negative (toàn vẹn) | A |
| DP-TIME-01 | DP-02 | Positive | A |
| DP-TIME-02 | DP-02 | Edge/Known-gap | B/C |
| DP-TIME-03 | DP-01 | Positive | B/C |
| CC-TIME-01 | CC-03 | Positive | B/C |
| CC-TIME-02 | CC-03 | Edge case | B/C |
| CC-TIME-03 | CC-03 | Positive | B/C |
| RC-TIME-01 | RC-03 | Positive | A |
| RC-TIME-02 | RC-03 | Edge + Negative | A |
| RC-TIME-03 | RC-03/06 | Positive | A |
| RC-TIME-04 | RC-04 | Positive | A |
| RC-TIME-05 | RC-04 | Edge case | A |
| RC-TIME-06 | RC-06 | Positive | A |
| PA-TIME-01 | PA-07 | Positive | B/C |
| PA-TIME-02 | PA-07 | Edge case | B/C |
| PA-TIME-03 | PA-07 | Negative | B/C |
| PA-TIME-04 | PA-11 | Negative + Edge | B/C |
| PA-TIME-05 | PA-11 | Edge case | B/C |
| PA-TIME-06 | PA-12 | Positive | C |
| PA-TIME-07 | PA-12 | Edge case | B/C |

**Tổng: 31 test case**, bao phủ toàn bộ các nhóm assumption thời gian được yêu cầu (TC-01..05, TL-01..06, DP-01/02, CC-01/03, RC-03/04/06, PA-07, PA-11/12), mỗi giá trị số có ít nhất một test positive và một test edge/boundary tại ranh giới tick, cùng các test negative cho các ngưỡng NACK theo PA-09/PA-11.
