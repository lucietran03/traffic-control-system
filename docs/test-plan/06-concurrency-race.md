# 06. Test Plan: Race Condition / Concurrency / Input dồn dập

Tài liệu này chứa test case hồi quy (regression) cho các bug tinh vi liên quan
đến **thứ tự sự kiện theo thời gian** (timing/race) đã từng bị phát hiện và
sửa trong `app/intersection/src/lx_fsm.c`, `app/railway/src/rlx_fsm.c`,
`app/shared/src/qnet_utils.c`, và `app/central/src/c_main.c` /
`c_operator.c`. Khác với các category test-plan khác (functional, boundary,
negative...), category này **không** kiểm tra "hệ thống làm đúng gì" mà kiểm
tra "hệ thống có làm đúng khi hai (hoặc nhiều) sự kiện gần như trùng thời
điểm không" — tức là các lỗi chỉ xuất hiện khi thứ tự thực thi giữa nhiều
luồng/tick trùng vào đúng một cửa sổ hẹp.

## Quy ước môi trường

Mỗi test case ghi rõ môi trường cần dùng:

- **(A) 1 node đơn**: một tiến trình (`lx_main`, `rlx_main`, hoặc `c_main`)
  chạy độc lập trên một máy/VM QNX, không cần IPC liên-node. Dùng cho các
  race chỉ liên quan tới nội bộ một FSM (ví dụ `lx_fsm.c` tự đấu với chính
  nó giữa luồng bàn phím và luồng server/watchdog).
- **(B) Nhiều node cùng máy QNX**: nhiều tiến trình (`c_main` + vài `lx_main`
  + vài `rlx_main`) chạy trên **cùng một** máy/VM QNX, giao tiếp qua
  `name_attach`/`name_open` nội bộ (không cần `TRAFFIC_NODE_MAP`). Dùng cho
  race liên quan tới IPC giữa Central và Lx/RLx nhưng độ trễ mạng thực không
  quan trọng.
- **(C) Nhiều máy/VM QNX qua mạng thật**: theo topology trong
  `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` (`VM_x86_Target01/02/03` +
  `TRAFFIC_NODE_MAP`). Dùng khi cần độ trễ Qnet thật để mở rộng cửa sổ race
  (một số race quá hẹp để bắt được trên loopback nội bộ nhưng lộ ra rõ hơn
  qua mạng thật với độ trễ/jitter cao hơn).

## Vì sao race condition khó tái hiện 100%, và cách giảm thiểu

1. **Lệ thuộc lịch trình hệ điều hành (scheduling)**: thứ tự thực thi giữa
   luồng bàn phím (`lx_sensor.c`/`rlx_sensor.c`/`c_operator.c`), luồng server
   (`ipc_server_run()`), luồng client IPC (`ipc_client_thread_main()`), và
   luồng watchdog không được đảm bảo bởi bất kỳ hợp đồng nào trong code —
   chúng chỉ tình cờ trùng khớp khi người test bấm phím đúng lúc. Trên một
   máy tải nhẹ, cửa sổ race có thể chỉ rộng vài mili-giây.
2. **Độ phân giải tick cố định**: `lx_fsm` tick mỗi 100 ms
   (`LX_PHASE_TICK_MS`), `rlx_fsm` tick mỗi 1000 ms — một sự kiện bàn phím
   đến "ngay trước" hay "ngay sau" một tick phụ thuộc vào việc bấm phím rơi
   vào đúng nửa mili-giây nào của chu kỳ tick đó, thứ mà tay người không thể
   căn chính xác từng lần.
3. **Log chỉ có độ phân giải giây**: `c_logger_log()` (central) đóng dấu thời
   gian bằng `strftime("%Y-%m-%d %H:%M:%S")` — không đủ mịn để xác nhận thứ
   tự hai sự kiện xảy ra cách nhau dưới 1 giây chỉ bằng cách đọc log. Vì vậy
   nhiều test case dưới đây xác minh bằng **trạng thái cuối cùng** (qua
   `status_report_payload_t`/HMI in trên C1, hoặc qua các dòng
   `printf` tức thời của `lx_signal.c`/`rlx_signal.c` — các dòng này in
   ngay khi gọi, không có timestamp giây, nên vẫn dùng được để xác nhận thứ
   tự tương đối trong cùng một lần chạy) thay vì cố gắng chứng minh thứ tự
   chính xác bằng timestamp.
4. **Giảm thiểu bằng lặp lại nhiều lần**: mỗi test case ghi rõ số lần lặp tối
   thiểu khuyến nghị (thường N ≥ 10) để tăng xác suất trúng cửa sổ race ít
   nhất một lần. Nếu có điều kiện, ưu tiên bấm phím bằng script gửi ký tự
   qua pipe/`expect`/`tmux send-keys` với `sleep` phân đoạn mili-giây thay vì
   gõ tay, vì tay người không lặp lại được độ trễ dưới ~150-200 ms một cách
   nhất quán — với các bước yêu cầu độ trễ nhỏ hơn mức đó, xem đây là
   "cố gắng bấm càng nhanh càng tốt, lặp lại N lần" chứ không phải một mốc
   thời gian tuyệt đối tay có thể đảm bảo.
5. **Không tái hiện được sau N lần lặp không đồng nghĩa hết bug**: ghi nhận
   là "không quan sát được lỗi trong N lần" (pass tạm thời), không kết luận
   "đã chứng minh không có race" — đây là giới hạn cố hữu của kiểm thử race
   condition thủ công, không phải thiếu sót của test plan.

---

## Nhóm A — Pedestrian latch / recall race (`lx_fsm.c`)

### TC-RACE-1: Bấm lại nút người đi bộ cùng phía trong lúc đang WALK
- **Loại**: Edge case
- **Tại sao là race**: `lx_fsm_latch_pedestrian_request()` chỉ set
  `ped_recall[side]=1` khi `ped_serving_mask` đã có bit của `side` đó (tức
  đang WALK/FDW). Nếu người test bấm nút *trong đúng cửa sổ* giữa lúc
  `ped_phase` chuyển sang `PED_PHASE_WALK` (tick báo `lx_signal_show_walk`)
  và trước khi `ped_phase_elapsed_ms` đạt `LX_WALK_MS` (6000 ms), bug cũ sẽ
  làm mất yêu cầu vì `ped_latched[side]` "đã là 1 sẵn" nên coi như không có
  gì để làm.
- **Liên quan**: `lx_fsm_latch_pedestrian_request()` +
  `lx_fsm_ped_service_tick_locked()`, field `ped_recall[4]` (`lx_fsm.h`
  dòng ~108-119, `lx_fsm.c` dòng 425-438, 172-194).
- **Môi trường**: (A) 1 node `lx_main` đơn.
- **Chuẩn bị**: Khởi động `lx_main` với `mode=PEAK_FIXED` (mặc định). Đợi
  phase vào `PHASE_ARTERIAL_GREEN` (log `SIGNAL -> ARTERIAL GREEN`).
- **Các bước**:
  1. Bấm `1` (pedestrian side 0) — log phải in `PED SIGNAL side 0 -> WALK`
     gần như ngay lập tức.
  2. Trong vòng 1-2 giây sau khi thấy dòng `WALK` (tức còn đang giữa
     `LX_WALK_MS`=6000 ms), bấm lại `1` một lần nữa.
  3. Đợi đủ chu kỳ WALK (6 s) + FLASHING_DONT_WALK (4 s) = 10 s cho tới khi
     thấy `PED SIGNAL side 0 -> DONT_WALK`.
  4. Tiếp tục quan sát: vì `ped_recall[0]` đã được set ở bước 2, ngay khi
     phase quay lại `PHASE_ARTERIAL_GREEN` lần kế tiếp (hoặc cùng phase nếu
     nó đủ dài), phải thấy một chu kỳ WALK/FDW **mới** tự khởi động cho side
     0 mà không cần bấm nút lần 3.
- **Kết quả mong đợi**: Yêu cầu bấm lần 2 không bị mất — một chu kỳ
  WALK→FDW→DONT_WALK thứ hai cho side 0 tự chạy sau khi chu kỳ đầu hoàn tất,
  không cần thao tác thêm.
- **Lưu ý**: Vì `LX_WALK_MS`/`LX_FLASHING_DONT_WALK_MS` cố định (không phải
  cửa sổ hẹp mili-giây), test này **tái hiện được gần như 100%** mỗi lần chạy
  miễn bấm lần 2 rơi vào khoảng 6-10 giây sau lần 1 — không cần lặp N lần,
  nhưng nên chạy tối thiểu 3 lần để loại trừ thao tác bấm nhầm.

### TC-RACE-2: Bấm lại nút cùng phía đúng ngay trước tick hoàn tất FLASHING_DONT_WALK
- **Loại**: Edge case (biên hẹp nhất của cùng bug với TC-RACE-1)
- **Tại sao là race**: Đây là cửa sổ hẹp hơn nhiều so với TC-RACE-1: bug cũ
  (nếu tái phát do regression) có thể chỉ lộ ra ở đúng tick cuối cùng của
  FDW — `lx_fsm_ped_service_tick_locked()` đọc `fsm->ped_recall[side]` **tại
  đúng tick** `ped_phase_elapsed_ms >= LX_FLASHING_DONT_WALK_MS` để quyết
  định latch lại hay clear hẳn. Nếu nút được bấm ở tick ngay trước đó
  (99 ms trước khi tick hoàn tất chạy) so với ngay sau tick đó, kết quả phải
  khác nhau một cách nhất quán, nhưng cả hai đường đều phải giữ đúng semantics
  (không rơi vào khoảng giữa gây mất dữ liệu).
- **Liên quan**: `lx_fsm.c` dòng 172-194 (nhánh `PED_PHASE_FLASHING_DONT_WALK`).
- **Môi trường**: (A) 1 node `lx_main` đơn.
- **Chuẩn bị**: Bấm `1` để bắt đầu WALK cho side 0. Theo dõi log để ước lượng
  thời điểm chuyển sang FDW (`PED SIGNAL side 0 -> FLASHING_DONT_WALK`, xảy
  ra ở giây thứ 6 sau lần bấm đầu).
- **Các bước**:
  1. Sau khi thấy dòng `FLASHING_DONT_WALK`, đợi gần đúng 4 giây (theo đồng
     hồ tay/stopwatch) rồi bấm `1` lại **càng sát mốc 4 giây càng tốt** (thử
     trong khoảng 3.8-4.0 giây sau khi thấy dòng FDW).
  2. Ghi lại chính xác dòng log tiếp theo xuất hiện: hoặc `DONT_WALK` (nếu
     bấm rơi vào trước tick hoàn tất — request được giữ lại qua
     `ped_recall`) hoặc một chu kỳ WALK mới lập tức nối tiếp (nếu tick hoàn
     tất đã chạy xong và `ped_latched[0]` đã bị dọn về 0 trước khi phím kịp
     tới, khi đó bấm phím sau đó latch lại bình thường qua đường
     `ped_latched[side]=1` chứ không qua `ped_recall`).
- **Kết quả mong đợi**: Dù rơi vào nhánh nào, **không bao giờ** được có
  trường hợp side 0 kết thúc ở `DONT_WALK` vĩnh viễn mà không có một chu kỳ
  WALK mới nào chạy sau đó — tức yêu cầu bấm lần 2 luôn được phục vụ, chỉ
  khác ở việc nó được phục vụ "nối liền ngay" (qua `ped_recall`) hay "y hệt
  một request mới" (qua `ped_latched`).
- **Lưu ý**: Cửa sổ chính xác này rất hẹp (dưới ~100 ms) và tay người không
  bấm chính xác được — lặp lại tối thiểu **N=10 lần**, mỗi lần thử canh thời
  điểm bấm lệch nhau vài trăm ms quanh mốc 4 giây, để tăng khả năng ít nhất
  một lần rơi đúng vào tick biên.

### TC-RACE-3: Bấm dồn dập liên tục (≥5 lần trong 1 giây) cùng một phía xuyên suốt cả WALK và FDW
- **Loại**: Edge case
- **Tại sao là race**: Đây là dạng "spam nút" thực tế nhất (người đi bộ mất
  kiên nhẫn bấm liên tục) — kiểm tra `ped_recall[side]` không bị ghi đè sai
  hay đặt lại nhiều lần gây hiệu ứng phụ (không có bug nào phát sinh từ việc
  set `ped_recall[side]=1` nhiều lần liên tiếp, vì đó là idempotent, nhưng
  cần xác nhận thực nghiệm rằng dồn dập không tạo ra 2 chu kỳ WALK "nợ" thay
  vì đúng 1 chu kỳ nợ).
- **Liên quan**: `ped_recall[4]`, `lx_fsm_latch_pedestrian_request()`.
- **Môi trường**: (A) 1 node `lx_main` đơn.
- **Chuẩn bị**: Đợi `PHASE_ARTERIAL_GREEN`.
- **Các bước**:
  1. Bấm `1` một lần để bắt đầu WALK side 0.
  2. Trong suốt 10 giây tiếp theo (toàn bộ WALK+FDW), bấm `1` liên tục,
     khoảng 5-10 lần, rải rác không đều (một số lần cách nhau <200 ms, một số
     lần cách nhau vài giây).
  3. Đợi hết chu kỳ đầu (`DONT_WALK`).
- **Kết quả mong đợi**: Đúng **một** chu kỳ WALK/FDW thứ hai chạy tiếp theo
  cho side 0 (không phải 5-10 chu kỳ, không phải 0 chu kỳ) — chứng minh
  dồn dập nhiều lần vẫn coi là một request "đang chờ" duy nhất, đúng
  semantics TL-06 "coalesce repeated presses".
- **Lưu ý**: Tái hiện ổn định (~100%) vì không phụ thuộc cửa sổ hẹp — chạy
  tối thiểu 3 lần để chắc chắn số lượng chu kỳ phục vụ luôn là 1, không dao
  động giữa các lần chạy.

### TC-RACE-4: Bấm đồng thời nhiều phía khác nhau trong cùng một tick 100ms
- **Loại**: Edge case
- **Tại sao là race**: `ped_serving_mask` được tính một lần khi
  `ped_phase == PED_PHASE_NONE` bằng cách gộp toàn bộ `ped_latched[]` tương
  thích với phase hiện tại (`compatible_mask`). Nếu hai phím (`1` và `2`,
  cùng thuộc arterial-compatible) được gõ trong cùng một tick 100 ms trước
  khi `lx_fsm_ped_service_tick_locked()` chạy, cả hai phải được gộp vào
  **cùng một** `ped_serving_mask` và phục vụ đồng thời trong cùng một chu kỳ
  WALK/FDW — nếu có bug thứ tự khóa (`fsm->lock`) giữa hai lần gọi
  `lx_fsm_latch_pedestrian_request()` từ luồng bàn phím và lần đọc trong
  tick, một trong hai có thể bị bỏ sót khỏi `compatible_mask` của chu kỳ này
  và phải chờ tới chu kỳ ARTERIAL_GREEN kế tiếp.
- **Liên quan**: `lx_fsm_ped_service_tick_locked()` dòng 129-158 (tính
  `compatible_mask`), khóa `fsm->lock`.
- **Môi trường**: (A) 1 node `lx_main` đơn.
- **Chuẩn bị**: Đợi vào `PHASE_ARTERIAL_GREEN` ngay từ đầu chu kỳ (log vừa in
  `SIGNAL -> ARTERIAL GREEN`), để có tối đa thời gian trước lần tick tiếp
  theo.
- **Các bước**:
  1. Gõ nhanh liên tiếp `1` rồi `2` (cả hai đều là side arterial-compatible)
     trong cùng một cú gõ bàn phím càng nhanh càng tốt (dưới 100 ms giữa hai
     phím nếu dùng script gửi input; nếu gõ tay, cố gắng gõ 2 ký tự dính
     liền nhau nhất có thể, ví dụ gõ "12" rồi Enter).
  2. Quan sát log `PED SIGNAL side 0 -> WALK` và `PED SIGNAL side 1 -> WALK`.
- **Kết quả mong đợi**: Cả hai dòng WALK cho side 0 và side 1 xuất hiện
  **trong cùng một chu kỳ** (cùng lúc bắt đầu, cùng lúc chuyển FDW, cùng lúc
  DONT_WALK) — không có trường hợp side 1 phải chờ một chu kỳ riêng biệt sau
  đó dù được bấm gần như đồng thời với side 0.
- **Lưu ý**: Nếu hai phím rơi vào hai tick 100 ms khác nhau (rất dễ xảy ra
  với tay người), side 2 hợp lệ sẽ đợi chu kỳ kế tiếp theo đúng thiết kế
  ("A side that latches mid-sequence... is picked up the next time a
  sequence starts") — đây **không phải** là bug. Chỉ coi là fail nếu cả hai
  phím chắc chắn rơi cùng 1 tick (dùng script gửi input đồng thời qua cùng
  một `write()` để đảm bảo) mà vẫn bị tách thành 2 chu kỳ. Lặp lại N ≥ 10
  lần với script để tăng khả năng cả hai phím rơi đúng cùng tick.

---

## Nhóm B — Override request/renew/cancel race (`lx_fsm.c`)

### TC-RACE-5: REQUEST_OVERRIDE đúng lúc pedestrian clearance sắp phục vụ xong
- **Loại**: Edge case
- **Tại sao là race**: Đây là race đã được sửa gần đây trong
  `lx_fsm_on_phase_timer()`: đoạn code re-validate `OVR_PENDING_CLEARANCE ->
  OVR_ACTIVE` phải chạy **SAU** `lx_fsm_ped_service_tick_locked()` trong
  cùng một tick, vì `ped_clearance_active` chỉ được xoá bên trong hàm đó.
  Nếu thứ tự bị đảo ngược (bug cũ), override bị kích hoạt trễ đúng một tick
  (100 ms) — không nguy hiểm về an toàn nhưng sai so với đặc tả "kích hoạt
  ngay khi clearance kết thúc". Test này buộc override phải được yêu cầu
  đúng vào tick cuối cùng của FDW để phơi bày lỗi thứ tự nếu nó tái phát.
- **Liên quan**: `lx_fsm_on_phase_timer()` dòng 904-939 (thứ tự gọi
  `lx_fsm_ped_service_tick_locked()` trước, rồi mới check
  `OVR_PENDING_CLEARANCE`); `lx_fsm_on_request_override()` dòng 713-728.
- **Môi trường**: (A) 1 node `lx_main` đơn (cần cả bàn phím `lx_sensor` để
  bấm ped và một cách gửi `MSG_REQUEST_OVERRIDE` — dùng `c_operator`'s lệnh
  `o` nếu chạy kèm `c_main`, hoặc một client test gửi IPC trực tiếp nếu có).
  Nếu chỉ có 1 node Lx độc lập không có Central, dùng môi trường (B) với một
  `c_main` tối thiểu để có lệnh `o`.
- **Chuẩn bị**: Bấm `1` để bắt đầu WALK side 0. Từ `c_operator`, chuẩn bị sẵn
  chuỗi nhập cho lệnh `o` (Lx number, target movement, duration) nhưng chưa
  Enter dòng cuối.
- **Các bước**:
  1. Theo dõi log `PED SIGNAL side 0 -> FLASHING_DONT_WALK` (đánh dấu bắt
     đầu 4 giây FDW).
  2. Gửi `MSG_REQUEST_OVERRIDE` (qua `o` trên `c_operator`, chọn đúng Lx,
     target movement = arterial, duration hợp lệ ví dụ 30000) sao cho nó
     đến Lx **trong khoảng 3.5-4.0 giây** sau khi thấy dòng FDW ở bước 1 —
     tức càng gần tick cuối cùng của FDW càng tốt.
  3. Quan sát phản hồi ACK/ACK_PENDING và log tiếp theo trên Lx.
- **Kết quả mong đợi**: Nếu request đến trong khi FDW vẫn còn hiệu lực, Lx
  trả `RESULT_ACK_PENDING` và `override_substate = OVR_PENDING_CLEARANCE`;
  ngay tại **tick mà** `DONT_WALK` xuất hiện (kết thúc FDW), override phải
  chuyển sang `OVR_ACTIVE` **trong cùng tick đó** — không có tick "chờ thêm"
  nào giữa lúc `ped_clearance_active` về 0 và lúc override kích hoạt. Xác
  nhận qua `status_report_payload_t.override_active` hoặc qua việc phase
  vật lý chuyển sang di chuyển được override ngay khi tới ranh giới ALL_RED
  kế tiếp mà không bị trễ thêm nửa chu kỳ.
- **Lưu ý**: Vì đơn vị tick là 100 ms, việc canh đúng "tick cuối cùng" bằng
  tay khó chính xác tuyệt đối. Lặp lại tối thiểu N ≥ 10 lần, mỗi lần lệch
  thời điểm gửi request vài trăm ms quanh mốc 4 giây, để đảm bảo ít nhất một
  lần request rơi đúng vào 1-2 tick cuối của FDW.

### TC-RACE-6: Gửi 2 REQUEST_OVERRIDE liên tiếp cực nhanh cho cùng một Lx
- **Loại**: Edge case (negative-ish, nhưng thuộc race vì phụ thuộc thứ tự
  đến của 2 message)
- **Tại sao là race**: `lx_fsm_on_request_override()` chặn override thứ hai
  bằng cách kiểm tra `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE` khi
  request thứ hai được xử lý. Nếu hai request được gửi gần như đồng thời
  (trước khi request đầu tiên kịp được `lx_fsm.c` xử lý và chuyển
  `supervisory` sang `SUPERVISORY_CENTRAL_OVERRIDE`), có nguy cơ (nếu không
  khoá đúng) cả hai đều thấy `supervisory != CENTRAL_OVERRIDE` cùng lúc và
  cả hai đều được ACK, ghi đè state của nhau. `fsm->lock` phải tuần tự hoá
  hai lần gọi `lx_fsm_on_request_override()` (chạy trên cùng server thread
  của Lx nên vốn đã tuần tự — bug chỉ có thể phát sinh nếu có đường gọi
  song song nào khác được thêm vào sau này) để đảm bảo cái đến sau luôn thấy
  đúng trạng thái do cái đến trước để lại.
- **Liên quan**: `lx_fsm_on_request_override()` (bắt đầu dòng 731 trong
  `lx_fsm.c` hiện tại), nhánh `NACK_REASON_OUT_OF_RANGE` ở dòng 747 khi đã
  `SUPERVISORY_CENTRAL_OVERRIDE` - lưu ý: đây là một cách dùng
  `NACK_REASON_OUT_OF_RANGE` khác với nhánh mode-validation mới thêm ở
  `lx_fsm_on_set_mode()` (dòng 710) - hai nhánh khác hàm, chỉ trùng tên
  reason code.
- **Môi trường**: (B) nhiều node cùng máy (1 `c_main` + 1 `lx_main`) để có
  đường IPC thật qua `ipc_client_post()`/`MsgSend()`, không phải gọi hàm C
  trực tiếp.
- **Chuẩn bị**: Lx đang ở `SUPERVISORY_NORMAL_OPERATION` (không override nào
  đang chạy).
- **Các bước**:
  1. Trên `c_operator`, gõ `o`, nhập Lx target, movement=arterial,
     duration=60000, Enter để gửi request đầu tiên.
  2. **Ngay lập tức** (trong vòng dưới 1 giây, càng nhanh càng tốt) lặp lại
     `o` với cùng target Lx, movement=connector, duration=60000, gửi request
     thứ hai.
  3. Quan sát cả hai phản hồi ACK/NACK in ra qua `c_comm.c`'s
     `on_command_reply` (dòng log `C1: SET_MODE/REQUEST_OVERRIDE to N ->
     RESULT`).
- **Kết quả mong đợi**: Đúng một trong hai request được `RESULT_ACK` (hoặc
  `RESULT_ACK_PENDING` nếu ped clearance đang chạy), request còn lại phải
  nhận `RESULT_NACK` với `NACK_REASON_OUT_OF_RANGE`. Tuyệt đối không được có
  trường hợp cả hai đều ACK (double-processing) — `override_target_movement`
  cuối cùng trên Lx phải khớp với đúng request được ACK, không bị request
  bị NACK ghi đè lên.
- **Lưu ý**: Vì hai request phải đi qua network/IPC tuần tự trên cùng một
  `ipc_client_thread_main()` của `c_main` (chỉ có 1 luồng client), thứ tự
  gửi đi thường được giữ nguyên tới đích, nên test này có tỷ lệ tái hiện khá
  cao (>80%) miễn 2 lệnh `o` được gõ liên tiếp nhanh trong cùng phiên
  `c_operator`. Chạy tối thiểu N ≥ 5 lần để xác nhận ổn định.

### TC-RACE-7: RENEW_OVERRIDE gửi đúng lúc override sắp hết hạn tự nhiên
- **Loại**: Edge case
- **Tại sao là race**: `override_remaining_ms` đếm ngược mỗi tick 100 ms; khi
  `<= LX_PHASE_TICK_MS` thì `lx_fsm_terminate_override_locked()` được gọi
  ngay trong `lx_fsm_on_phase_timer()`, chuyển `override_substate = OVR_NONE`
  và `supervisory` về `NORMAL_OPERATION`. Nếu `MSG_RENEW_OVERRIDE` đến đúng
  tick đó, `lx_fsm_on_renew_override()` yêu cầu
  `override_substate == OVR_ACTIVE` — nếu tick hết hạn chạy trước khi renew
  message được xử lý (nhưng cả hai đều chạy tuần tự dưới `fsm->lock` nên
  không có data race thật sự, chỉ là race về **thứ tự đến**), renew phải bị
  NACK sạch sẽ (`NACK_REASON_UNKNOWN_TARGET`) thay vì crash hoặc để lại state
  nửa vời (`override_remaining_ms` khác 0 nhưng `substate == OVR_NONE`).
- **Liên quan**: `lx_fsm_on_phase_timer()` dòng 895-902 (expiry),
  `lx_fsm_on_renew_override()` dòng 740-763.
- **Môi trường**: (B) nhiều node cùng máy (`c_main` + `lx_main`).
- **Chuẩn bị**: Gửi `REQUEST_OVERRIDE` với `duration_ms` rất ngắn, ví dụ
  `500` (nửa giây — vẫn hợp lệ vì `(0, 300000]`) để dễ canh thời điểm hết
  hạn.
- **Các bước**:
  1. Gửi `o` với duration_ms=500. Ghi lại thời điểm gửi (t0).
  2. Ngay sau đó, gửi `r` (RENEW_OVERRIDE, extend_duration_ms=0) sao cho nó
     tới Lx **khoảng t0+400 đến t0+600 ms** — tức đúng quanh mốc hết hạn dự
     kiến.
  3. Lặp lại nhiều lần với độ trễ gửi renew rải đều quanh mốc 500 ms (400,
     450, 500, 550, 600 ms) để bao phủ cả hai phía của biên.
- **Kết quả mong đợi**: Với renew đến trước khi hết hạn: `RESULT_ACK`, override
  tiếp tục chạy với thời hạn mới. Với renew đến sau khi đã hết hạn (log
  `override cleared/expired - running safe clearance sequence` đã xuất
  hiện): `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET`, và trạng thái Lx phải ở
  đúng `SUPERVISORY_NORMAL_OPERATION`/`OVR_NONE` — không có trạng thái lơ
  lửng nào ở giữa.
- **Lưu ý**: Cửa sổ chính xác quanh 500 ms rất khó canh tay chính xác dưới
  ±100 ms. Lặp N ≥ 10 lần với các độ trễ gửi khác nhau để phủ cả hai nhánh
  của biên hết hạn.

### TC-RACE-8: CANCEL_OVERRIDE gửi đúng lúc override tự hết hạn
- **Loại**: Edge case
- **Tại sao là race**: Tương tự TC-RACE-7 nhưng với `CANCEL_OVERRIDE`: cả
  expiry (trong tick) và cancel (khi request đến) đều gọi
  `lx_fsm_terminate_override_locked()` — hàm này phải **idempotent** dưới
  khoá `fsm->lock` (gọi hai lần liên tiếp không được gây lỗi kép, không
  được in "override cleared" hai lần cho cùng một override, không được
  giảm `override_remaining_ms` xuống âm/underflow uint32).
- **Liên quan**: `lx_fsm_on_cancel_override()` dòng 765-779 (guard
  `override_substate != OVR_PENDING_CLEARANCE && != OVR_ACTIVE` ->
  `NACK_REASON_UNKNOWN_TARGET`), `lx_fsm_terminate_override_locked()` dòng
  205-216.
- **Môi trường**: (B) nhiều node cùng máy.
- **Chuẩn bị**: Giống TC-RACE-7 — override với `duration_ms=500`.
- **Các bước**:
  1. Gửi `o` duration_ms=500 (t0).
  2. Gửi `c` (CANCEL_OVERRIDE, cùng target) sao cho đến quanh t0+500ms, lặp
     lại với các độ trễ rải quanh mốc 500 ms giống TC-RACE-7.
- **Kết quả mong đợi**: Nếu cancel đến trước khi hết hạn: `RESULT_ACK`, log
  "override cleared/expired" in đúng **một lần**. Nếu cancel đến sau khi đã
  tự hết hạn: `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET` (vì đã là
  `OVR_NONE`), và log "override cleared/expired" cũng chỉ in đúng **một
  lần** (từ nhánh expiry) — không bao giờ in hai lần cho cùng một sự kiện
  hết hạn, không có `override_remaining_ms` âm/tràn số.
- **Lưu ý**: Lặp N ≥ 10 lần rải độ trễ quanh mốc hết hạn.

### TC-RACE-9: Railway preemption (crossing status) đến đúng lúc REQUEST_OVERRIDE đang bay tới
- **Loại**: Edge case
- **Tại sao là race**: CC-02 yêu cầu không được cấp override xung đột với
  railway preemption, và nếu preemption bắt đầu **trong khi** một override
  đang active, nó phải bị evict qua `lx_fsm_terminate_override_locked()`
  (`lx_fsm_on_crossing_status()` dòng 792-798). Nếu `MSG_REQUEST_OVERRIDE`
  và `MSG_CROSSING_STATUS` (do RLx gửi khi phát hiện tàu) đến Lx gần như
  cùng lúc, thứ tự xử lý quyết định kết quả: nếu override được xử lý trước
  khi preemption tới, nó phải bị evict ngay khi preemption đến sau đó (bug
  đã biết: CC-02 "cannot grant override conflicting with active railway
  preemption" — request phải bị NACK nếu preemption đã tới **trước**, và
  phải bị evict sạch nếu preemption tới **sau**).
- **Liên quan**: `lx_fsm_on_request_override()` dòng 705-709
  (`NACK_REASON_RAILWAY_CONFLICT`), `lx_fsm_on_crossing_status()` dòng
  781-833.
- **Môi trường**: (B) nhiều node cùng máy: `c_main` + 1 `lx_main` + 1
  `rlx_main` (đây là kịch bản demo, `crossing_status` thực tế trong hệ thống
  đến từ RLx gửi trực tiếp, không qua Central — kiểm tra lại luồng dây đúng
  trong `ipc_msg.h`/`c_server.c` để xác nhận đường đi thật; nếu
  `MSG_CROSSING_STATUS` chỉ được RLx gửi tới Lx liên quan chứ không qua
  C1, cần cấu hình đúng target mapping cho topology demo).
- **Chuẩn bị**: Lx ở `NORMAL_OPERATION`, RLx ở `RLX_OPEN`.
- **Các bước**:
  1. Chuẩn bị sẵn lệnh `o` trên `c_operator` (nhưng chưa Enter dòng cuối,
     hoặc chuẩn bị gửi ngay khi có tín hiệu).
  2. Trên `rlx_sensor`, bấm `0` (TRAIN_APPROACHING hướng 0) để RLx bắt đầu
     chuỗi WARNING → CLOSING → CLOSED, dẫn tới việc RLx gửi
     `MSG_CROSSING_STATUS(CLOSED)` cho Lx liên quan.
  3. **Ngay khi vừa bấm `0`** (trong vòng dưới 1 giây), gửi `o` trên
     `c_operator` yêu cầu override cho đúng Lx đó.
  4. Lặp lại theo chiều ngược: override trước, rồi bấm `0` ngay sau (trong
     vòng dưới 1 giây) để override kịp được ACK trước khi preemption tới.
- **Kết quả mong đợi**:
  - Trường hợp preemption tới **trước** override: override phải bị
    `RESULT_NACK`/`NACK_REASON_RAILWAY_CONFLICT`.
  - Trường hợp override được ACK **trước** rồi preemption mới tới: override
    phải bị evict qua safe clearance (log "override cleared/expired") và
    `supervisory` chuyển sang `SUPERVISORY_RAILWAY_PREEMPTION`, không bao
    giờ giữ nguyên `SUPERVISORY_CENTRAL_OVERRIDE` song song với preemption.
  - Không có trạng thái nào mà cả `override_active` và railway preemption
    cùng "active" đồng thời trên `status_report_payload_t`.
- **Lưu ý**: Do phụ thuộc độ trễ thật của chuỗi WARNING (5s trước khi vào
  CLOSING) trước khi `CROSSING_STATUS(CLOSED)` thực sự được gửi (không phải
  ngay khi bấm `0`), cửa sổ race thực tế rộng hơn dự kiến ban đầu — cần xác
  nhận trước bằng cách đọc `RLX_WARNING_TO_CLOSING_MS`
  (5000ms)/`RLX_CLOSING_DEADLINE_MS` (15000ms) trong `rlx_timer.h` để canh
  đúng lúc `CROSSING_STATUS` thật sự được gửi (không phải lúc bấm phím) rồi
  gửi override quanh mốc đó. Lặp N ≥ 5 lần cho mỗi chiều.

### TC-RACE-10: Watchdog trip xảy ra đúng lúc override đang ACTIVE
- **Loại**: Edge case
- **Tại sao là race**: `lx_fsm_report_watchdog_trip()` được gọi từ **luồng
  watchdog riêng** (không phải server thread), và nó tự lấy `fsm->lock` rồi
  gọi `lx_fsm_terminate_override_locked()` nếu đang
  `SUPERVISORY_CENTRAL_OVERRIDE` — đây là race thật sự giữa hai luồng khác
  nhau (không chỉ thứ tự message như các case trên): nếu watchdog trip đến
  đúng lúc server thread đang ở giữa `lx_fsm_on_phase_timer()` xử lý tick
  override (ví dụ đang đếm `override_remaining_ms`), `fsm->lock` phải đảm
  bảo hai thao tác này tuần tự hoá đúng, không được để lại
  `override_substate` không nhất quán với `supervisory`.
- **Liên quan**: `lx_fsm_report_watchdog_trip()` dòng 465-477 (gọi
  `lx_fsm_terminate_override_locked()` trực tiếp từ luồng watchdog, độc lập
  server thread — xem doc comment giải thích lý do compliance-audit fix).
- **Môi trường**: (A) 1 node `lx_main` đơn (watchdog nội bộ tự trip, không
  cần Central).
- **Chuẩn bị**: Kích hoạt một override đang `OVR_ACTIVE` (qua `c_main` nếu
  có, hoặc gọi trực tiếp path test nếu có hook). Nếu không có cách kích hoạt
  watchdog thật (`lx_watchdog.c` chưa hoàn thiện ở một số bản), dùng bất kỳ
  cơ chế demo trip nào có sẵn trong `lx_main.c`/`lx_watchdog.c` để giả lập
  main loop bị treo.
- **Các bước**:
  1. Với override đang `OVR_ACTIVE`, kích hoạt điều kiện khiến watchdog trip
     (ví dụ block server thread nhân tạo nếu có hook debug, hoặc đợi watchdog
     timeout tự nhiên nếu môi trường hỗ trợ).
  2. Ngay khi watchdog trip xảy ra, đồng thời (nếu có thể) gửi thêm một
     `MSG_RENEW_OVERRIDE`/`MSG_CANCEL_OVERRIDE` tới đúng lúc đó để tăng khả
     năng chồng lấp hai luồng ghi vào cùng field.
- **Kết quả mong đợi**: Sau khi watchdog trip, trạng thái cuối cùng phải là
  `SUPERVISORY_FAULT_SAFE`, `override_substate == OVR_NONE`,
  `override_remaining_ms == 0` — nhất quán hoàn toàn, không có tổ hợp field
  mâu thuẫn nào (ví dụ `supervisory == FAULT_SAFE` nhưng
  `override_substate == OVR_ACTIVE`). Bất kỳ renew/cancel nào gửi cùng lúc
  phải nhận `NACK`/`FAULT_ACTIVE` hoặc `UNKNOWN_TARGET`, không panic/crash
  tiến trình.
- **Lưu ý**: Đây là race hai-luồng thật sự (khác các case IPC message ở
  trên) nên khó dựng kịch bản thủ công nếu không có hook debug sẵn để ép
  watchdog trip đúng lúc. Nếu môi trường test không có cách ép trip thủ
  công, coi test này là "chạy khi có cơ hội tự nhiên" và ghi nhận qua log
  dài hạn (chạy hệ thống nhiều giờ với override liên tục được renew, theo
  dõi xem có bao giờ watchdog trip trùng với override active không) thay vì
  một kịch bản bấm phím xác định.

---

## Nhóm C — Mode / offset boundary race (`lx_fsm.c`)

### TC-RACE-11: Đổi mode liên tục nhiều lần trước khi tới ranh giới ALL_RED đầu tiên
- **Loại**: Edge case
- **Tại sao là race**: `lx_fsm_on_set_mode()` chỉ ghi vào
  `fsm->pending_mode`/`mode_change_pending`, áp dụng thật sự chỉ diễn ra tại
  `lx_fsm_advance_phase_locked()`'s `PHASE_ALL_RED_A_TO_B`/`PHASE_ALL_RED_B_TO_A`.
  Nếu operator đổi ý nhiều lần (gửi `SET_MODE` với các giá trị khác nhau)
  trước khi phase kịp chạm ranh giới ALL_RED tiếp theo, `pending_mode` bị
  ghi đè liên tục — đúng thiết kế chỉ giá trị **cuối cùng** được áp dụng,
  nhưng cần xác nhận không có giá trị trung gian nào "lọt" vào do thứ tự xử
  lý sai.
- **Liên quan**: `lx_fsm_on_set_mode()` dòng 689-729 trong `lx_fsm.c`
  hiện tại (đã dịch xuống sau khi TC-02/TC-03's `offset_extra_hold_ms`
  fix thêm code vào `lx_fsm_apply_offset_locked()` phía trên),
  `lx_fsm_advance_phase_locked()` dòng 239-242, 306-309.
- **Môi trường**: (B) `c_main` + 1 `lx_main`.
- **Chuẩn bị**: Lx đang ở giữa `PHASE_ARTERIAL_GREEN` (còn nhiều giây trước
  khi hết 48s), mode hiện tại = PEAK_FIXED.
- **Các bước**:
  1. Gửi `m` chọn mode=1 (OFF_PEAK_SENSOR).
  2. Ngay sau đó (trong vòng 1-2 giây, chắc chắn còn ở ARTERIAL_GREEN), gửi
     `m` lại chọn mode=0 (PEAK_FIXED).
  3. Lặp lại đổi qua đổi lại 3-4 lần trong vòng vài giây, kết thúc bằng một
     giá trị xác định (ví dụ mode=1) là lệnh cuối cùng.
  4. Đợi phase đi hết ARTERIAL_YELLOW + ALL_RED_A_TO_B (khoảng 4s+2s) để
     mode được áp dụng.
- **Kết quả mong đợi**: Ngay tại `PHASE_ALL_RED_A_TO_B`, `fsm->mode` phải
  chuyển đúng bằng **giá trị của lệnh `m` cuối cùng đã gửi** (mode=1 trong
  ví dụ trên) — không phải giá trị nào ở giữa, không bị "rớt" về giá trị cũ.
  Xác nhận qua `status_report_payload_t.mode` sau ranh giới.
- **Lưu ý**: Vì mỗi `SET_MODE` được xử lý tuần tự qua cùng một server thread
  của Lx, thứ tự áp dụng ổn định — tái hiện gần 100%. Chạy 3 lần để xác
  nhận.

### TC-RACE-12: Đổi mode lặp lại ngay tại cả hai ranh giới ALL_RED trong một chu kỳ nhanh
- **Loại**: Edge case (regression trực tiếp cho bug đã sửa)
- **Tại sao là race**: Đây là regression test trực tiếp cho bug đã từng bị
  "rớt 1 nhánh" — bản vá cũ chỉ áp dụng `mode_change_pending` ở
  `PHASE_ALL_RED_A_TO_B` mà quên `PHASE_ALL_RED_B_TO_A` (hoặc ngược lại).
  Test này buộc mode phải đổi hai lần liên tiếp, mỗi lần đúng ngay trước một
  ranh giới ALL_RED khác nhau trong cùng phiên chạy, để xác nhận **cả hai**
  nhánh code (dòng 239-242 và dòng 306-309 trong `lx_fsm_advance_phase_locked()`)
  đều hoạt động, không chỉ một nhánh.
- **Liên quan**: `lx_fsm_advance_phase_locked()` dòng 236-242 (nhánh A_TO_B)
  và dòng 301-309 (nhánh B_TO_A) — comment tại dòng 302-305 ghi rõ đây là
  "fixed after a re-verification pass caught that this got dropped".
- **Môi trường**: (B) `c_main` + 1 `lx_main`.
- **Chuẩn bị**: Mode ban đầu = PEAK_FIXED, Lx ở đầu `PHASE_ARTERIAL_GREEN`.
- **Các bước**:
  1. Ngay khi vào `PHASE_ARTERIAL_GREEN`, gửi `m` mode=1
     (OFF_PEAK_SENSOR) — mode này sẽ áp dụng tại `ALL_RED_A_TO_B` sắp tới.
  2. Xác nhận qua status report: `mode` đã đổi thành OFF_PEAK_SENSOR ngay
     sau `ALL_RED_A_TO_B` (log `SIGNAL -> CONNECTOR GREEN` xuất hiện sau đó
     xác nhận đã qua ranh giới).
  3. Ngay khi vào `PHASE_CONNECTOR_GREEN`, gửi `m` mode=0 (PEAK_FIXED) —
     mode này sẽ áp dụng tại `ALL_RED_B_TO_A` sắp tới.
  4. Xác nhận qua status report sau khi `SIGNAL -> ARTERIAL GREEN` xuất hiện
     lần kế tiếp.
- **Kết quả mong đợi**: Cả hai lần đổi mode đều được áp dụng đúng — lần 1 áp
  dụng tại `ALL_RED_A_TO_B`, lần 2 áp dụng tại `ALL_RED_B_TO_A`. Không có
  trường hợp lần đổi thứ 2 (nhánh B_TO_A) bị bỏ qua trong khi lần 1 (nhánh
  A_TO_B) hoạt động bình thường — nếu chỉ nhánh A_TO_B hoạt động, đó chính
  là bug đã từng xảy ra tái phát.
- **Lưu ý**: Test này không phụ thuộc cửa sổ mili-giây (chỉ cần gửi `m` bất
  kỳ lúc nào trong khoảng thời gian dài của mỗi green phase), nên tái hiện
  ổn định — chạy tối thiểu 3 lần đủ để tự tin, không cần lặp N=10.

### TC-RACE-13: SET_TIMING_PROFILE (offset) và SET_MODE gửi gần như đồng thời, cùng chờ áp dụng tại ranh giới
- **Loại**: Edge case
- **Tại sao là race**: Cả `offset_apply_pending` (TC-02/TC-03) và
  `mode_change_pending` (SC-01A) đều là các "pending flag" chỉ được tiêu thụ
  tại một điểm an toàn cụ thể: `offset_apply_pending` chỉ tại lúc **vừa vào**
  `PHASE_ARTERIAL_GREEN` (dòng 339-345), còn `mode_change_pending` tại
  **ranh giới ALL_RED trước đó** (dòng 239-242/306-309). Nếu cả hai được gửi
  gần như đồng thời trong cùng phase hiện tại, thứ tự tiêu thụ hai flag này
  chồng lấp lên cùng một lần chuyển phase — cần xác nhận không có tương tác
  phụ (ví dụ áp dụng offset bị tính sai vì `green_elapsed_ms` bị mode change
  ảnh hưởng, hay ngược lại).
- **Liên quan**: `lx_fsm_apply_offset_locked()` (dòng 564-614),
  `lx_fsm_advance_phase_locked()` (dòng 227-347) — cả hai pending-flag
  patterns dùng chung "safe boundary" nhưng khác điểm kích hoạt
  (ALL_RED_A_TO_B/B_TO_A cho mode, đầu ARTERIAL_GREEN cho offset).
- **Môi trường**: (B) `c_main` + 1 `lx_main`, mode = PEAK_FIXED.
- **Chuẩn bị**: Lx đang ở `PHASE_CONNECTOR_GREEN` (để cả hai lệnh có thời
  gian chờ qua `ALL_RED_B_TO_A` rồi vào `ARTERIAL_GREEN` mới, nơi offset
  được áp dụng).
- **Các bước**:
  1. Gửi `t` (SET_TIMING_PROFILE) chọn chain chứa Lx này, với offset bất kỳ
     hợp lệ (< `LX_CYCLE_LENGTH_MS`=90000).
  2. Ngay sau đó (trong vòng 1 giây), gửi `m` đổi mode (dù TC-02/TC-03 chỉ
     áp dụng khi `mode == MODE_PEAK_FIXED`, vẫn gửi để kiểm tra tương tác dù
     mode đổi sang OFF_PEAK_SENSOR ngay sau đó khiến offset bị bỏ qua theo
     đúng thiết kế "chỉ áp dụng khi PEAK_FIXED").
  3. Theo dõi `SIGNAL -> ALL RED (B to A)` rồi `SIGNAL -> ARTERIAL GREEN`.
- **Kết quả mong đợi**:
  - Nếu mode vẫn là PEAK_FIXED tại thời điểm vào `ARTERIAL_GREEN` mới: offset
    phải được áp dụng đúng một lần (không bị bỏ qua, không áp dụng hai lần).
  - Nếu mode đã đổi sang OFF_PEAK_SENSOR trước khi vào `ARTERIAL_GREEN` đó:
    theo đúng thiết kế, offset **không** được áp dụng (vì
    `lx_fsm_apply_offset_locked()` guard `mode != MODE_PEAK_FIXED` return
    sớm) — `offset_apply_pending` phải được clear (dòng 343) dù không áp
    dụng, để không bị áp dụng nhầm một chu kỳ sau đó khi mode đổi lại về
    PEAK_FIXED.
  - Không được có trường hợp cả hai pending flag đều "kẹt" mãi không bao giờ
    được tiêu thụ.
- **Lưu ý**: Chạy cả hai thứ tự gửi (timing trước/mode trước) và cả hai giá
  trị mode kết quả, tổng cộng thử tối thiểu 4 tổ hợp, mỗi tổ hợp 2-3 lần.

---

## Nhóm D — Railway occupancy window race (`rlx_fsm.c`)

### TC-RACE-14: Hai tàu đến gần như đồng thời từ hai hướng khác nhau
- **Loại**: Edge case
- **Tại sao là race**: `register_window()` quét `fsm->windows[]` (chỉ có
  `RLX_MAX_OCCUPANCY_WINDOWS`=2 slot) để tìm slot trống hoặc slot cùng
  hướng đã có. Nếu phím `0` và `1` được gõ gần như đồng thời (cả hai đều đi
  qua `rlx_sensor_reader_thread` — **chỉ một luồng đọc bàn phím**, nên về
  bản chất được tuần tự hoá bởi `scanf()` đọc từng ký tự một; race thật sự ở
  đây là giữa luồng bàn phím và luồng tick 1Hz (`rlx_fsm_on_tick()`) đang
  chạy song song và có thể đang xử lý `enter_train_present()`/gate polling
  đúng lúc phím thứ hai tới). Cần xác nhận cả hai hướng đều được track đúng
  slot, không bị hướng thứ hai ghi đè nhầm lên slot của hướng thứ nhất.
- **Liên quan**: `register_window()` dòng 83-104,
  `RLX_MAX_OCCUPANCY_WINDOWS` = 2 (`rlx_fsm.h` dòng 27).
- **Môi trường**: (A) 1 node `rlx_main` đơn.
- **Chuẩn bị**: RLx ở `RLX_OPEN`.
- **Các bước**:
  1. Gõ liên tiếp thật nhanh `0` rồi `1` (dưới 1 giây giữa hai phím, lý
     tưởng là gõ dính "01" rồi Enter nếu `scanf(" %c", ...)` cho phép đọc
     từng ký tự không cần Enter — xác nhận theo hành vi thực tế của
     `rlx_sensor_reader_thread`).
  2. Theo dõi log: `flashers ON (train approaching, direction 0)` phải xuất
     hiện, RLx chuyển sang `RLX_WARNING`. Phím `1` tiếp theo rơi vào nhánh
     self-loop (`RLX_WARNING`/`CLOSING`/`RECLOSING`) — không in thêm dòng
     "flashers ON" thứ hai (thiết kế hiện tại không in thêm log cho self-loop,
     chỉ gọi `register_window()` âm thầm) nhưng phải có 2 window active.
  3. Đợi đủ 5s (`RLX_WARNING_TO_CLOSING_MS`) rồi 15s deadline
     (`RLX_CLOSING_DEADLINE_MS`) cho tới khi gate xác nhận đóng — log
     `train signal PROCEED for direction 0` và
     `train signal PROCEED for direction 1` phải **cả hai** xuất hiện (xem
     `check_closing_or_reclosing_complete()` loop qua toàn bộ
     `windows[]` active).
- **Kết quả mong đợi**: Cả hai hướng đều có occupancy window active và đều
  nhận `train signal PROCEED` khi gate đóng xong — không có hướng nào bị bỏ
  sót do ghi đè slot.
- **Lưu ý**: Vì `RLX_MAX_OCCUPANCY_WINDOWS`=2 khớp đúng số hướng tối đa
  (RC-01 chỉ có 2 hướng), test này không phụ thuộc cửa sổ mili-giây gắt gao
  — chạy 3 lần để xác nhận ổn định.

### TC-RACE-15: Tàu dồn dập vượt quá 2 slot / lặp lại cùng hướng liên tục trong khi 2 window đã đầy
- **Loại**: Edge case (boundary + race kết hợp)
- **Tại sao là race**: `register_window()` không có slot thứ 3 — khi cả hai
  slot đã `active=1` với 2 hướng khác nhau, một lần gọi thêm (dù cùng hướng
  hay giả định có hướng thứ 3) rơi vào vòng lặp thứ hai không tìm được slot
  trống và bị "ignored defensively" (comment dòng 102-104). Cần xác nhận
  hành vi này không làm hỏng 2 window đang có, và việc gọi lại cùng hướng đã
  tồn tại (refresh, không phải hướng mới) vẫn hoạt động đúng dù gọi dồn dập.
- **Liên quan**: `register_window()` dòng 83-104 (đặc biệt nhánh
  refresh dòng 87-92 vs. nhánh slot mới dòng 93-101).
- **Môi trường**: (A) 1 node `rlx_main` đơn.
- **Chuẩn bị**: RLx ở `RLX_OPEN`.
- **Các bước**:
  1. Gõ `0` để mở window hướng 0.
  2. Gõ dồn dập liên tục `0 0 0 0 0` (5 lần, cách nhau <300ms mỗi lần) — mỗi
     lần phải refresh `remaining_ms` cho slot hướng 0 (không tạo thêm
     window mới, `active_window_count` không được tăng quá 1 cho hướng
     này).
  3. Gõ `1` một lần để mở window hướng 1 (đủ 2 slot).
  4. Gõ dồn dập `0 1 0 1 0 1` xen kẽ (mỗi lần cách nhau <300ms) — cả hai đều
     là refresh của slot đã tồn tại, không phải slot mới.
- **Kết quả mong đợi**: Sau toàn bộ chuỗi trên, `active_window_count` phải
  đúng bằng 2 (không phải 5, không phải 0, không tăng vọt do dồn dập) — mỗi
  lần gõ trùng hướng chỉ refresh `remaining_ms`, không tạo slot mới lãng
  phí. Khi cả hai window hết hạn (hoặc gate đóng xong), đúng 2 lần
  `train signal PROCEED` được in (không phải 5 lần, không phải 1 lần).
- **Lưu ý**: Kịch bản chỉ có 2 hướng vật lý theo RC-01 nên không thể test
  "hướng thứ 3 thật" bằng bàn phím (`rlx_sensor.c` chỉ có phím `0`/`1`) —
  test này tập trung vào việc dồn dập **refresh** không làm tràn số lượng
  window, đây là phần thực tế có thể kiểm chứng qua bàn phím. Tái hiện ổn
  định, chạy 3 lần.

### TC-RACE-16: Hai occupancy window hết hạn gần như cùng lúc — gate chỉ mở khi CẢ HAI hết hạn
- **Loại**: Edge case (regression cho RC-04 invariant)
- **Tại sao là race**: `rlx_fsm_on_tick()`'s nhánh `RLX_TRAIN_PRESENT` giảm
  `remaining_ms` cho **từng** window active mỗi tick, và chỉ gọi
  `enter_opening()` khi `active_window_count == 0` — nghĩa là ngay cả khi cả
  hai window hết hạn ở **cùng một tick** (vì cả hai được đăng ký gần như
  đồng thời ở TC-RACE-14 nên có `remaining_ms` khởi tạo giống hệt nhau), vòng
  `for` phải giảm/dọn cả hai window trong cùng tick đó trước khi kiểm tra
  `active_window_count == 0` — nếu có bug thứ tự (kiểm tra count ngay sau
  khi dọn window đầu tiên thay vì sau khi dọn hết cả vòng lặp), gate có thể
  mở sớm khi window thứ hai trên thực tế cũng đã hết nhưng logic không đảm
  bảo đã xử lý xong toàn bộ mảng.
- **Liên quan**: `rlx_fsm_on_tick()` dòng 372-391 (vòng `for` dọn hết
  `windows[]` trước, kiểm tra `active_window_count == 0` sau vòng lặp, không
  bên trong vòng lặp) — đúng theo comment "reopening fires only when the
  COUNT of active windows reaches zero, never on a single window's expiry
  alone while another remains active" (dòng 384-386).
- **Môi trường**: (A) 1 node `rlx_main` đơn.
- **Chuẩn bị**: Gõ `0` rồi ngay lập tức `1` (như TC-RACE-14) để cả hai window
  có `remaining_ms` khởi tạo gần như đồng thời (cả hai đều bắt đầu đếm 20s
  từ lúc vào `RLX_TRAIN_PRESENT`, không phải từ lúc đăng ký — xem
  `enter_train_present()` dòng 178-196: mọi window có `remaining_ms==0` khi
  vào TRAIN_PRESENT được gán đồng loạt `RLX_OCCUPANCY_WINDOW_MS`).
- **Các bước**:
  1. Gõ `0` rồi `1` nhanh trong lúc RLx còn `RLX_OPEN`/`RLX_WARNING`.
  2. Đợi hết chuỗi WARNING(5s)→CLOSING(gate motion 3s)→CLOSED→
     EXPECTED_ARRIVAL(20s)→TRAIN_PRESENT — tại thời điểm vào
     `RLX_TRAIN_PRESENT`, cả hai window có `remaining_ms = 20000` (bằng
     nhau, vì cùng được gán trong cùng lệnh gọi `enter_train_present()`).
  3. **Không** gõ thêm phím nào khác trong lúc chờ, để đảm bảo hai window
     đếm ngược song song và hết hạn ở cùng tick.
  4. Theo dõi log tick-by-tick (nếu `rlx_main.c` có in debug elapsed, nếu
     không thì theo dõi 2 dòng `train signal PROCEED` đã in trước đó và chờ
     đúng 20 giây sau khi vào TRAIN_PRESENT).
- **Kết quả mong đợi**: Đúng tại giây thứ 20 sau khi vào TRAIN_PRESENT, cả
  hai window hết hạn cùng lúc và `commanding gates UP (simulated motion...)`
  (từ `enter_opening()`) chỉ được gọi **một lần duy nhất** ngay tại tick đó
  — không sớm hơn (ví dụ ở giây 19 khi lẽ ra vẫn phải còn 1 window active
  theo logic sai), không muộn hơn (không có tick "thừa" chờ thêm không cần
  thiết).
- **Lưu ý**: Vì cả hai window được khởi tạo cùng giá trị đếm ngược, chúng
  chắc chắn hết hạn cùng tick — test này tái hiện ổn định gần 100%, không
  cần lặp N lớn, nhưng nên chạy 3 lần để xác nhận thời điểm chính xác quan
  sát được qua đồng hồ tay khớp với 20s ± 1 tick.

### TC-RACE-17: TRAIN_APPROACHING hướng khác đến đúng lúc đang OPENING
- **Loại**: Edge case
- **Tại sao là race**: `rlx_fsm_simulate_train_approaching()`'s nhánh
  `RLX_OPENING` gọi `enter_reclosing()` — đây là chuyển trạng thái phụ
  thuộc chặt vào **đúng thời điểm** phím được bấm rơi vào state nào:
  `RLX_OPEN`/`RLX_TRAIN_PRESENT`/`RLX_OPENING` mỗi cái xử lý khác nhau. Cửa
  sổ `RLX_OPENING` chỉ kéo dài `RLX_GATE_MOTION_MS`=3000ms (gate đang mở) —
  đây là cửa sổ hẹp nhất trong toàn bộ FSM này để test phải bấm phím trúng.
  Nếu bấm trúng, gate phải hủy motion mở và quay lại đóng (`enter_reclosing`
  gọi lại `rlx_gate_command_close()`) mà không để lại trạng thái gate lơ
  lửng (nửa mở nửa đóng, cả hai cờ `confirmed_open`/`confirmed_closed` đều
  0 — đây là trạng thái hợp lệ tạm thời trong lúc motion đang chạy, nhưng
  phải resolve đúng sau đó).
- **Liên quan**: `rlx_fsm_simulate_train_approaching()` dòng 273-275
  (`case RLX_OPENING: enter_reclosing(fsm, direction);`), `enter_reclosing()`
  dòng 169-176.
- **Môi trường**: (A) 1 node `rlx_main` đơn.
- **Chuẩn bị**: Đưa RLx tới trạng thái sắp vào `RLX_OPENING`: gõ `0`, đợi đủ
  chuỗi WARNING→CLOSING→CLOSED→(20s)→TRAIN_PRESENT→(hết 20s occupancy,
  không gõ thêm gì)→ đúng lúc `commanding gates UP` xuất hiện (bắt đầu
  `RLX_OPENING`, kéo dài 3000ms).
- **Các bước**:
  1. Ngay khi thấy dòng `RLx: all train signals -> STOP (crossing
     reopening)` + `commanding gates UP (simulated motion, 3000 ms)`, bấm
     `1` (hướng khác) **trong vòng 3 giây** kể từ dòng log đó — càng sớm
     càng tốt sau khi thấy log, để chắc chắn rơi vào giữa cửa sổ 3000ms.
  2. Quan sát log tiếp theo: phải thấy `RLx: reclosing - aborting gate-open
     motion, flashers remain active` và `commanding gates DOWN`.
- **Kết quả mong đợi**: RLx chuyển sang `RLX_RECLOSING` (wire-visible là
  `CROSSING_WARNING` theo `map_to_crossing_state()`), gate được lệnh đóng
  lại, và sau khi gate xác nhận đóng (`check_closing_or_reclosing_complete()`),
  window cho hướng mới (hướng 1) nhận `train signal PROCEED`. Tuyệt đối
  không được có trường hợp RLx báo `CROSSING_OPEN` trong khi thực tế vừa có
  tàu mới được ghi nhận đang tới, và không được crash/kẹt ở trạng thái gate
  không xác định quá `RLX_CLOSING_DEADLINE_MS` (15s) mà không vào `RLX_FAULT`.
- **Lưu ý**: Cửa sổ 3000ms tương đối rộng so với các race khác nên tay người
  có thể canh được, nhưng vẫn nên lặp N ≥ 5 lần với thời điểm bấm rải đều
  trong khoảng 0-3000ms sau khi thấy log, để phủ cả đầu và cuối cửa sổ
  `RLX_OPENING`. Nếu bấm sau khi gate đã xác nhận mở (`RLX_OPEN` đã vào),
  hành vi đúng là mở window mới bình thường qua nhánh `RLX_OPEN` — không
  phải bug, chỉ là rơi ra ngoài cửa sổ test này.

### TC-RACE-18: Bấm fault-clear ('f') đúng lúc có tàu mới approaching tới
- **Loại**: Edge case
- **Tại sao là race**: `rlx_fsm_on_fault_clear()` và
  `rlx_fsm_simulate_train_approaching()` đều lấy `fsm->lock` nên tuần tự hoá
  đúng, nhưng cả hai đều đến từ **cùng một luồng bàn phím**
  (`rlx_sensor_reader_thread` xử lý cả phím `f` và `0`/`1` tuần tự qua cùng
  vòng lặp `scanf`) — điều cần kiểm chứng là ngữ nghĩa đúng khi 2 phím này
  được gõ sát nhau: theo code, khi `state == RLX_FAULT`,
  `rlx_fsm_simulate_train_approaching()`'s case `RLX_FAULT` là no-op (dòng
  277-280, "Latched until rlx_fsm_on_fault_clear() succeeds... approach
  events are ignored while faulted"). Nếu `f` được gõ ngay trước một phím
  `0`/`1`, thứ tự quyết định: nếu fault-clear thành công trước, tàu mới phải
  được ghi nhận bình thường (không bị nuốt); nếu gõ `0`/`1` trước khi `f`
  chạy xong (không thể vì cùng luồng tuần tự, nhưng cần verify), sự kiện
  approach đó phải bị bỏ qua hoàn toàn, không phải bị "treo" chờ xử lý sau
  khi fault-clear xong.
- **Liên quan**: `rlx_fsm_on_fault_clear()` dòng 289-318,
  `rlx_fsm_simulate_train_approaching()` dòng 277-280.
- **Môi trường**: (A) 1 node `rlx_main` đơn.
- **Chuẩn bị**: Đưa RLx vào `RLX_FAULT` (ví dụ bấm `x` để arm demo gate
  fault rồi bấm `0` để trigger một chu kỳ đóng gate thất bại xác nhận, dẫn
  tới `FAULT_GATE_CONFIRM_MISSING` sau `RLX_CLOSING_DEADLINE_MS`=15s). Đợi
  gate tự confirm open thật (`rlx_gate_poll_open()` phải trả 1) trước khi
  test — vì `rlx_fsm_on_fault_clear()` chỉ ACK khi `gates_confirmed_open()`
  đúng thật.
- **Các bước**:
  1. Với RLx đang `RLX_FAULT` và gate đã thật sự confirm open (không còn
     armed demo fault), gõ liên tiếp thật nhanh `f` rồi `0` (dưới 1 giây,
     `f` trước `0`).
  2. Quan sát output của `f`: `fault-clear result=... reason=...`.
  3. Quan sát ngay sau đó `0` có được ghi nhận hay không (log `flashers ON`
     phải xuất hiện nếu fault-clear ACK trước khi `0` được xử lý).
- **Kết quả mong đợi**: Vì `rlx_sensor_reader_thread` xử lý tuần tự từng ký
  tự qua một vòng lặp `while(scanf(...))` duy nhất, `f` luôn được xử lý xong
  (bao gồm cả lock/unlock) trước khi `0` được đọc — nên `0` phải luôn thấy
  trạng thái **sau khi** fault đã clear (`RLX_OPEN`), tức phải rơi vào nhánh
  `RLX_OPEN` bình thường và mở window mới, không bao giờ bị nuốt bởi nhánh
  `RLX_FAULT`. Nếu quan sát thấy `0` bị nuốt (không có `flashers ON` nào
  xuất hiện sau đó), đây là bug thật (vi phạm giả định tuần tự hoá của thiết
  kế single-threaded sensor reader).
- **Lưu ý**: Vì cả hai phím đi qua cùng một luồng đơn (không có race thật sự
  ở tầng OS/thread ở đây, chỉ là race về mặt kịch bản nghiệp vụ), test này
  tái hiện ổn định 100% — chủ yếu để tài liệu hoá invariant "không bao giờ
  nuốt sự kiện" hơn là để bắt bug ngẫu nhiên. Chạy 3 lần.

---

## Nhóm E — IPC queue / Central concurrency

### TC-RACE-19: Dồn dập lệnh operator liên tiếp làm gần đầy queue 16 slot của `ipc_client_post()`
- **Loại**: Edge case
- **Tại sao là race**: `ipc_client_queue_t` là ring buffer cố định
  `IPC_CLIENT_QUEUE_CAPACITY`=16, được tiêu thụ bởi **đúng một**
  `ipc_client_thread_main()` — mỗi job phải đi qua `name_open()` +
  `MsgSend()` chặn (blocking) trước khi job kế tiếp được lấy ra khỏi hàng
  đợi. Nếu operator gửi nhiều lệnh dồn dập hơn tốc độ luồng client tiêu thụ
  được (ví dụ một target không phản hồi khiến `MsgSend()` treo lâu, hoặc chỉ
  đơn giản gửi rất nhiều broadcast liên tiếp), hàng đợi có thể đầy và
  `ipc_client_post()` trả về `-1` — cần xác nhận hành vi drop được xử lý an
  toàn (log cảnh báo, không crash, không kẹt luồng operator vì
  `ipc_client_post()` không hề block).
- **Liên quan**: `ipc_client_post()` (`qnet_utils.c` dòng 357-385, đặc biệt
  guard `q->count == IPC_CLIENT_QUEUE_CAPACITY` dòng 368), các hàm
  `c_comm_send_*()`/`c_comm_broadcast_timing_profile()` (`c_comm.c`) đều log
  `"... dropped - outgoing queue full or stopping"` khi post thất bại.
- **Môi trường**: (B) nhiều node cùng máy: `c_main` + toàn bộ 6 `lx_main` +
  3 `rlx_main` (để có đủ 9 target thật, và để `MsgSend()` có khả năng chậm
  hơn nếu một vài target không phản hồi kịp — thử tắt bớt 1-2 `lx_main`
  trước khi test để mô phỏng target không phản hồi, khiến `name_open()`
  hoặc `MsgSend()` trong `ipc_client_thread_main()` mất thời gian dài hơn
  bình thường và làm nghẽn hàng đợi).
- **Chuẩn bị**: Tắt (không khởi động) `lx_main` cho L6 để mô phỏng một target
  "treo"/không tồn tại (khiến `name_open()` thất bại nhanh — để mô phỏng
  chậm thật sự cần một cách khác, ví dụ target tồn tại nhưng
  `ipc_server_run()` của nó bị block; nếu không dựng được kịch bản "chậm
  thật", vẫn có thể kiểm tra riêng phần "gửi dồn dập hơn 16 lệnh trước khi
  luồng client kịp rút bớt" bằng cách gửi thật nhanh).
- **Các bước**:
  1. Gõ liên tiếp thật nhanh (script gửi input nếu có thể, hoặc gõ tay hết
     mức) ≥ 17 lệnh `m`/`o`/`t` (mỗi lệnh `t` broadcast tạo ra 3
     `ipc_client_post()` cho chain 3 controller, nên chỉ cần 6 lệnh `t` liên
     tiếp là đủ vượt 16 job) trong vòng vài giây, không đợi phản hồi giữa
     các lệnh.
  2. Theo dõi `central_log.txt`/stdout để đếm số dòng
     `"... dropped - outgoing queue full or stopping"`.
- **Kết quả mong đợi**: Khi hàng đợi đầy, các job vượt quá 16 phải bị drop
  **có log rõ ràng** (không im lặng mất), tiến trình `c_main` không
  deadlock/crash, và luồng `c_operator_reader_thread` vẫn tiếp tục nhận lệnh
  tiếp theo bình thường ngay lập tức (không bị block chờ hàng đợi trống, vì
  `ipc_client_post()` trả về ngay `-1` chứ không chờ). Sau khi luồng client
  rút bớt hàng đợi, các lệnh gửi sau đó (không đầy nữa) phải được xử lý bình
  thường trở lại.
- **Lưu ý**: Rất khó ép hàng đợi đầy thật sự trên loopback nội bộ vì
  `MsgSend()` tới một target đang chạy bình thường thường trả lời rất nhanh
  (dưới vài ms) — luồng client rút hàng đợi nhanh hơn tốc độ gõ tay có thể
  tạo ra. Ưu tiên dùng môi trường (C) qua mạng thật (độ trễ Qnet cao hơn) và/
  hoặc tắt một số target để buộc `name_open()`/`MsgSend()` mất thời gian chờ
  timeout, tăng khả năng hàng đợi tích tụ. Nếu không đạt được đầy hàng đợi
  thật, hạ mục tiêu xuống "xác nhận không crash/không mất dữ liệu ở mức tải
  vừa phải" và ghi rõ trong báo cáo là chưa chứng minh được nhánh đầy hàng
  đợi. Lặp N ≥ 5 lần.

### TC-RACE-20: Operator gõ lệnh liên tục trong khi server thread xử lý heartbeat/status report dồn dập
- **Loại**: Edge case
- **Tại sao là race**: `c_mode_eng_t` (struct `ctx.mode_eng` trong
  `c_main.c`) được ghi bởi **hai luồng khác nhau** dưới cùng một
  `mode_eng_lock`: (1) server thread của `c_main`, trong vòng lặp
  `ipc_server_run()` xử lý `on_request()`
  (`MSG_STATUS_REPORT`/`MSG_HEARTBEAT`/`MSG_FAULT_REPORT`/
  `MSG_CROSSING_STATUS` → `c_server_record_*()`) và `on_pulse()`
  (`IPC_PULSE_HEARTBEAT_TICK` mỗi 1s → `c_watchdog_mon_tick()` +
  `c_hmi_render()`); và (2) luồng `c_operator_reader_thread`
  (`handle_set_mode()`/`handle_timing_profile()`/`handle_request_override()`/
  `handle_renew_override()`/`handle_cancel_override()`, mỗi hàm đều
  lock/unlock `mode_eng_lock` quanh việc đọc/ghi
  `controllers[idx].last_commanded_mode`/`override_in_flight`/
  `last_applied_profile_id`). Với 9 controller (6 Lx + 3 RLx) gửi
  status/heartbeat dồn dập gần như liên tục, server thread giữ
  `mode_eng_lock` rất thường xuyên (dù mỗi lần rất ngắn) — nếu operator gõ
  lệnh đúng lúc lock đang bị giữ, thao tác operator phải **chờ** (mutex
  block bình thường) chứ không được đọc/ghi dữ liệu cũ (torn read) hay bỏ
  qua lock.
- **Liên quan**: `c_main.c` dòng 37-51 (field + comment giải thích lý do
  thêm `mode_eng_lock`), dòng 60-89 (on_request handlers),
  dòng 114-117 (on_pulse: `c_watchdog_mon_tick()` + `c_hmi_render()`),
  `c_operator.c` mọi `handle_*()` (dòng 116-330, đều
  `pthread_mutex_lock(args->mode_eng_lock)`).
- **Môi trường**: (B) hoặc (C) đầy đủ: `c_main` + toàn bộ 6 `lx_main` + 3
  `rlx_main` đang chạy và gửi status report/heartbeat định kỳ thật (không
  giả lập) để tạo tải dồn dập tự nhiên lên server thread.
- **Chuẩn bị**: Đảm bảo tất cả 9 controller đang kết nối và gửi
  status/heartbeat đều đặn (quan sát `c_hmi_render()` cập nhật liên tục trên
  màn hình mỗi giây).
- **Các bước**:
  1. Trong khi hệ thống đang chạy đầy đủ 9 controller, gõ liên tục và nhanh
     nhiều lệnh operator khác nhau: `m` (đổi mode L1), rồi ngay `o` (override
     L2), rồi `r` (renew L2 nếu override vừa được accept), rồi `t` (broadcast
     timing profile chain 1), rồi `c` (cancel L2) — mỗi lệnh cách nhau dưới
     1 giây, không đợi phản hồi ACK/NACK giữa các lệnh.
  2. Lặp lại chuỗi trên liên tục trong khoảng 30-60 giây trong khi 9
     controller vẫn liên tục gửi status/heartbeat/crossing_status (tải nền
     tự nhiên không cần can thiệp gì thêm).
  3. Theo dõi HMI (`c_hmi_render()` output) và `central_log.txt` trong suốt
     quá trình.
- **Kết quả mong đợi**:
  - Không deadlock: cả `c_operator_reader_thread` và server thread phải
    tiếp tục tiến triển bình thường trong suốt 30-60 giây (HMI vẫn cập nhật
    mỗi giây, operator vẫn nhận phản hồi cho từng lệnh).
  - Không torn write: sau khi dừng gõ, đọc lại `mode_eng.controllers[]` (qua
    HMI hoặc log) phải thấy các field (`last_commanded_mode`,
    `override_in_flight`, `last_applied_profile_id`) ở trạng thái hợp lệ,
    nhất quán với lệnh cuối cùng đã gửi cho từng controller — không có giá
    trị "rác"/không thuộc enum hợp lệ nào (dấu hiệu của việc đọc giữa lúc
    đang ghi mà không có lock).
  - `c_watchdog_mon_tick()`'s missed-heartbeat bookkeeping vẫn chính xác:
    không controller nào bị báo "missed heartbeat" giả trong khi thực tế vẫn
    đang gửi heartbeat đều đặn, chỉ vì lock bị operator giữ lâu — xác nhận
    lock chỉ được giữ trong thời gian rất ngắn ở mỗi `handle_*()` (không có
    I/O chặn nào bên trong vùng lock).
- **Lưu ý**: Đây là race "tần suất cao, hậu quả tinh vi" (torn read/write)
  chứ không phải race "một lần trúng một lần trật" — nên chạy liên tục càng
  lâu càng tốt (khuyến nghị tối thiểu 5 phút liên tục, không chỉ 30-60 giây)
  để tăng số lần tranh chấp lock quan sát được, và lặp lại toàn bộ kịch bản
  N ≥ 3 phiên chạy riêng biệt (khởi động lại toàn bộ hệ thống giữa mỗi
  phiên) trước khi kết luận pass.
