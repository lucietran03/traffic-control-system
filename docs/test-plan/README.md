# Test Plan — Traffic Control System

Bộ tài liệu test thủ công cho toàn bộ hệ thống điều khiển giao thông phân tán
(EEET2588). Được chia thành 7 file theo loại kiểm thử để có thể chạy song
song bởi nhiều thành viên; mỗi file tự chứa đủ ngữ cảnh (bảng phím, hằng số
thời gian, quy ước môi trường) để không cần đọc lại toàn bộ source trước khi
test.

## Danh sách file

| # | File | Phạm vi | Loại test | Công cụ / phương pháp | Số case |
|---|---|---|---|---|---|
| 1 | [01-usecase-functional.md](01-usecase-functional.md) | UC-01..10 (usecase.md) — hành vi đầu-cuối theo từng use case | Functional / black-box | Bàn phím (`lx_sensor.c`/`rlx_sensor.c`/`c_operator.c`) + quan sát console/log | 43 |
| 2 | [02-state-machine-transition.md](02-state-machine-transition.md) | SC-01A/B/C, SC-02, SC-03A/B, SC-04A/B, SC-05 (STATE_CHARTS.md) — từng transition | State-machine transition | Bàn phím + quan sát trạng thái qua console/`c_hmi`; 1 số case cần chờ đúng mốc thời gian | 42 |
| 3 | [03-protocol-contract.md](03-protocol-contract.md) | 10 verb trong `ipc_msg.h` — mọi outcome ACK/ACK_PENDING/NACK(reason)/ERROR | Protocol / contract | Phần lớn qua `c_operator.c` (bàn phím); 1 số case sai định dạng cần **tool `test_client` riêng (chưa xây)** | 47 |
| 4 | [04-timing-assumptions.md](04-timing-assumptions.md) | Giá trị số/thời gian trong `system_assumptions_tables.md` (TC/TL/RC/PA/DP/CC) | Timing / performance | Đồng hồ bấm giờ tay hoặc đọc timestamp trong `central_log.txt` (không có đồng hồ chung, hệ thống dùng tick 100ms) | 31 |
| 5 | [05-fault-safety.md](05-fault-safety.md) | Watchdog, FAULT_SAFE, RC-06/09/10 gate fault | Fault-injection / safety | Bàn phím (phím `x`/`r`/`f` demo); 1 số case cần **debugger (gdb)** để mô phỏng treo thread hoặc gọi hàm trực tiếp không có UI | 23 |
| 6 | [06-concurrency-race.md](06-concurrency-race.md) | Race condition, input dồn dập, regression cho bug đã sửa | Concurrency / race | Bàn phím thao tác cực nhanh (nên script hóa nếu có thể), **lặp lại nhiều lần (N≥5-10)** để tăng khả năng bắt lỗi | 20 |
| 7 | [07-cross-node-integration.md](07-cross-node-integration.md) | Hành vi xuyên node qua Qnet thật (đa VM) | Integration / distributed | Nhiều VM QNX thật qua SSH + biến môi trường `TRAFFIC_NODE_MAP`, quan sát log đồng thời trên nhiều máy | 19 |
| | **Tổng** | | | | **225** |

## Quy ước môi trường (dùng chung mọi file)

- **(A) 1 node QNX đơn** — chỉ chạy 1 binary (vd `lx_main 1`), không cần node khác.
- **(B) Nhiều node trên CÙNG 1 máy QNX** — chạy nhiều binary cùng lúc, Qnet same-node, không cần `TRAFFIC_NODE_MAP`.
- **(C) Nhiều node trên NHIỀU máy/VM QNX thật qua mạng** — cần set `TRAFFIC_NODE_MAP` (xem `app/shared/README.md` mục "Cross-node resolution" và `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`).
- **(D) Cần công cụ `test_client` riêng (chưa tồn tại)** — một số case ở mức giao thức (payload sai định dạng, gửi thẳng bỏ qua validation của `c_operator.c`) không thể test qua UI hiện có. Đề xuất: 1 executable nhỏ tái sử dụng `qnet_utils.c`, tự dựng `ipc_request_t` tùy ý rồi gửi thẳng.

## Phát hiện quan trọng trong lúc thiết kế test (chưa sửa code, cần quyết định)

Các agent viết test độc lập với nhau nhưng nhiều lần tự phát hiện ra cùng một
vấn đề khi cố tìm cách tái hiện test case — dấu hiệu đáng tin cậy:

| # | Phát hiện | Mức độ | Ghi chú |
|---|---|---|---|
| 1 | **`RESULT_ACK` cho `MSG_REQUEST_FAULT_CLEAR` không thể tái hiện qua bàn phím** — không có đường nào gọi `rlx_gate_command_open()` khi đang ở state `RLX_FAULT`, nên nhánh "fault clear thành công" chỉ verify được bằng debugger | Xác nhận **độc lập bởi 3 agent** (section 1, 3, 5) | Không phải bug logic (validation vẫn đúng), chỉ là thiếu đường demo để test/trình diễn nhánh này |
| 2 | ~~`lx_fsm_local_fault_clear()` tồn tại nhưng không được nối vào bất kỳ trigger nào~~ — **ĐÃ SỬA**: `MSG_REQUEST_FAULT_CLEAR` nay được `lx_main.c` xử lý qua `lx_fsm_on_request_fault_clear()`, và `c_operator.c`'s phím `f` hỏi node type (0=Lx/1=RLx), nên Lx có đường thoát `FAULT_SAFE` thật qua Central mà không cần khởi động lại process | Đã đóng | Fix đi kèm một re-audit an toàn: resume đúng `RAILWAY_PREEMPTION` (không phải luôn `NORMAL_OPERATION`) nếu crossing kề bên vẫn chưa `OPEN` tại thời điểm clear (`fsm->last_crossing_state`) — xem `05-fault-safety.md` TC-FAULT-14b/14c |
| 3 | **DP-02 (tự động chuyển Peak/Off-Peak theo giờ) chưa nối vào runtime** — `c_mode_eng_select_mode()` không được gọi với giờ thật ở đâu cả, chỉ đổi mode bằng tay qua operator | Thấp | Không ảnh hưởng an toàn, chỉ là tự động hóa chưa hoàn thiện |
| 4 | **`rlx_comm_broadcast_crossing_status_if_changed()` không có resend/backfill** — Lx khởi động trễ bỏ lỡ 1 lần gửi sẽ không tự đồng bộ lại cho tới lần đổi trạng thái kế tiếp | Thấp | Giới hạn thiết kế đã biết, ghi vào test case như expected behavior |
| 5 | `NACK_REASON_PEDESTRIAN_ACTIVE` là dead code (định nghĩa nhưng không bao giờ set) | Thấp | Đã biết từ đợt audit trước, hành vi thật là `ACK_PENDING` |

## Cách dùng bộ tài liệu này

1. Build theo `../../README.md`/root `Makefile`, hoặc theo QNX Momentics (`docs/QNX_MOMENTICS_INTEGRATION.md`).
2. Chạy test theo thứ tự file 1 → 7 (độ phức tạp môi trường tăng dần: 1-2 chủ yếu môi trường A/B, 4-5 cần quan sát kỹ thời gian, 6 cần lặp lại nhiều lần, 7 bắt buộc môi trường C).
3. Với mỗi test case FAIL, đối chiếu lại đúng file:function được trích trong mục "Liên quan" trước khi báo lỗi.
