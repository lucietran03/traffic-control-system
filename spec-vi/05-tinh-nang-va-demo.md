[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · Chương 5/6 · [← An toàn và real-time](04-an-toan-va-real-time.md) · Tiếp theo: [Giả định và tổng kết →](06-gia-dinh-va-tong-ket.md)

---

## 23. Phân loại tính năng

### Core (bắt buộc để đạt yêu cầu đề bài)

- Thiết kế 6 giao lộ + chỗ giao đường sắt [REQ]
- Local controller cho mỗi giao lộ, hoạt động liên tục [REQ]
- Central controller có giám sát/status/lệnh [REQ]
- Local tự điều khiển đèn của mình, sống sót khi mất central/comm [REQ]
- 3 nhóm chuỗi vận hành: fixed / sensor-driven / advanced [REQ]
- Boom gate + đèn nhấp nháy + train signal, fail-to-red [REQ][CLARIF]
- Sensor mô phỏng bằng phím bấm [REQ]
- Nhiều QNX node với process riêng biệt [REQ]
- Hiển thị trạng thái dạng terminal [REQ]
- Bộ controller Pass-minimum PoC (`L1`, `L2`, `RL1` tại `I1`, `I2`, `RC1`) [REQ]

### HD-Target — ĐÃ CHỐT, SẼ LÀM (theo Mục 4.2, không phải tuỳ chọn)

- Phần mở rộng `L3`/`L4`/`RL2` (Mục 4.2)
- Advance/queue sensor + logic ngừng cấp nhu cầu/pha drain (Mục 16) — trả lời trực tiếp yêu cầu "state and justify... congestion control" của đề bài
- Phối hợp green-wave trên arterial (Mục 20) — nội dung phối hợp phân tán + phân tích timing thật sự
- Quyền local từ chối lệnh không an toàn kèm báo NACK (Mục 18) — lập luận an toàn cụ thể, demo được
- Watchdog/supervisor process trên mỗi node (Mục 17) — process QNX nhỏ kiểm tra controller chính còn sống không, nếu không thì ép output về an toàn ngay
- Override bị giới hạn, tự hết hạn từ Central kèm quy tắc phụ thuộc rõ ràng (Mục 22)

### Tuỳ chọn — SẼ LÀM SAU nếu còn thời gian (chưa cam kết, KHÔNG phải huỷ)

Đây là các ý tưởng team **chưa quyết định bỏ**, chỉ là chưa nằm trong kế hoạch build bắt buộc ngay bây giờ — nếu hoàn thành sớm phần HD-Target ở trên, đây là danh sách ưu tiên làm tiếp:

- Mở rộng PoC lên mạng lưới đầy đủ 9 controller (`L5/L6` tại `I5/I6`, `RL3` tại `RC3`)
- Thiết kế làn rẽ riêng + đèn mũi tên protected cho `I1`/`I2` (Phương án B, xem Mục 5b)
- Giao diện đồ hoạ (GUI) thay vì chỉ terminal
- Đối chiếu bằng exit sensor thời gian thực thay cho timer cố định (xem đánh đổi đã chấp nhận ở Mục 14) — sensor vật lý đã có sẵn trong thiết kế (Mục 9, mục 5), chỉ chưa nối vào logic

### Loại bỏ hẳn — KHÔNG làm, kể cả về sau (quyết định cuối, khác với mục Tuỳ chọn ở trên)

| Ý tưởng | Lý do loại bỏ |
|---|---|
| Mô phỏng xe đến theo thống kê/ngẫu nhiên | Đây là dự án *hệ thống điều khiển*; sự kiện sensor (mô phỏng bằng phím bấm theo [REQ]) mới là mô hình input đúng — xây 1 traffic simulator thêm cả 1 subsystem riêng cần kiểm chứng mà không có điểm số nào cho nó |
| Pha "scramble" (all-red) cho người đi bộ | Giao lộ có đèn tín hiệu thông thường mặc định dùng parallel walk; scramble thêm 1 mode ma trận xung đột riêng biệt mà không có cơ sở yêu cầu |
| Đèn tín hiệu/sensor riêng theo từng làn (trong Core) | Đề bài chỉ yêu cầu 2 làn/chiều với turn lane "may have" — chi tiết theo từng làn nhân độ phức tạp mà không có yêu cầu tương ứng; phân tích đầy đủ về đánh đổi ở Mục 5b |
| Mô hình nhu cầu 4 khung giờ (sáng/ngày/chiều/đêm) | 2 khung (Peak/Off-Peak) đã đủ để kiểm chứng mọi cơ chế chuyển mode và phối hợp; thêm khung thứ 4 chỉ là thêm số để giải thích, không thêm thiết kế |
| Mô hình lệch hướng nhu cầu / arrival dạng burst | Không có hành vi sensor/actuator nào phụ thuộc vào thống kê arrival được mô hình hoá — đây chỉ là độ chân thực mô phỏng mà không ảnh hưởng gì tới logic điều khiển được đánh giá |
| Thuật toán tái đồng bộ green-wave dần dần sau sự kiện tàu | Đường phục hồi/reprofile bình thường đã khôi phục phối hợp; 1 giai đoạn resync riêng là 1 state machine thứ hai giải quyết vấn đề mà state machine đầu đã giải quyết rồi |
| Phối hợp local-to-local cho các connector qua đường sắt | Connector ngắn và bị chi phối bởi railway pre-emption; phối hợp timing của chúng không có lợi ích di chuyển đáng kể để mô hình hoá |
| `C1` được override thiết bị đường sắt trong tình huống khẩn cấp | Bị loại bỏ rõ ràng vì lý do an toàn — không quyền remote nào được phép bỏ qua interlock gate-confirmed-closed; khớp với khuyến nghị chính giảng viên đã nêu |

---

## 24. Kịch bản demo

| # | Kịch bản | Kiểm chứng điều gì |
|---|---|---|
| 1 | Chạy `PEAK_FIXED` bình thường tại `I1`/`I2` (`L1`/`L2`) | Chuỗi 2 pha Core, khoảng clearance |
| 2 | `OFF_PEAK_SENSOR` phản ứng với 1 sự kiện xe mô phỏng | Gia hạn theo sensor, trạng thái nghỉ mặc định ở đường chính |
| 3 | Bấm nút người đi bộ → WALK → clearance → tiếp tục | Toàn bộ vòng đời người đi bộ, cơ chế latch |
| 4 | Lệnh `SET_MODE`/`SET_TIMING_PROFILE` từ Central chỉ áp dụng tại ranh giới an toàn | Command abstraction, áp dụng đúng ranh giới an toàn |
| 5 | Chuỗi tàu tiếp cận đầy đủ tại `RC1` (`RL1`) | Warning → gate đóng → xác nhận đã đóng → train proceed |
| 6 | Tàu đang chiếm dụng crossing, phát hiện tàu thứ 2 trước khi tàu đầu thoát | Giao lộ liền kề giữ đỏ hướng về crossing, invariant không mở gate (chồng cửa sổ chiếm dụng) |
| 7 | Crossing mở lại + pha drain | Phục hồi, giải phóng hàng xe, quay lại nhóm pha đã dùng trước đó |
| 8 | Bơm lỗi boom gate khi có tàu đang tới | Train signal bị ép `STOP`, báo lỗi, không có proceed không an toàn |
| 9 | Tắt `C1` giữa demo | `DEGRADED_LOCAL`, mọi local vẫn vận hành an toàn, tự chủ |
| 10 | Bật lại `C1` | Local tự khởi động resync (đẩy trạng thái lên, không phải nhận lệnh xuống) |
| 11 | `REQUEST_OVERRIDE(CLEAR_ROUTE)` trong lúc WALK người đi bộ đang diễn ra | Override được xếp hàng/phụ thuộc đúng, không cắt clearance |
| 12 | `REQUEST_OVERRIDE` được yêu cầu trong lúc railway pre-emption đang diễn ra tại cùng giao lộ | Bị từ chối ở local (NACK), an toàn đường sắt thắng |
| 13 | Cảnh báo hàng xe từ advance sensor gần 1 crossing đang đóng | Logic ngừng cấp nhu cầu (giữ đỏ hướng về crossing) |
| 14 | Offset green-wave qua `I1–I3` (`L1`–`L3`, phần mở rộng HD) | Phối hợp arterial, offset tính từ khoảng cách/tốc độ |
| 15 | Kill process của 1 `Lx` | Watchdog/supervisor ép output về trạng thái an toàn tại node đó |

---

## 25. Input mô phỏng

Một bộ phím bấm gợi nhớ duy nhất, cấu trúc `<loại><vị trí><chi tiết>`. Ký tự/số theo sau xác định **vị trí vật lý** (giao lộ hoặc crossing); phím bấm được định tuyến nội bộ tới controller (`Lx`/`RLx`) của vị trí đó:

| Tiền tố | Ý nghĩa | Ví dụ |
|---|---|---|
| `V<giao lộ><approach>` | Sensor phát hiện xe kích hoạt | `V1N` = phát hiện xe, approach Bắc của `I1` (do `L1` xử lý) |
| `Q<giao lộ><approach>` | Advance/queue sensor kích hoạt | `Q2W` = cảnh báo hàng xe, approach Tây của `I2` (do `L2` xử lý) |
| `P<giao lộ><bên>` | Bấm nút người đi bộ | `P1E` = yêu cầu người đi bộ, lối qua Đông của `I1` (do `L1` xử lý) |
| `T<crossing><hướng>` | Tàu đang tới | `T1A` = tàu đang tới `RC1` từ hướng A; `T1B` = hướng B (do `RL1` xử lý) |
| `FG<crossing>` | Bơm lỗi gate/sensor | `FG1` = lỗi tại gate của `RC1` (do `RL1` xử lý) |
| `FL<giao lộ>` | Bơm lỗi phần cứng đèn local | `FL2` = lỗi tại `I2` (do `L2` xử lý) |
| `M` | Đổi qua lại `PEAK_FIXED`/`OFF_PEAK_SENSOR` (console vận hành) | — |
| `O<giao lộ><hướng>` | Yêu cầu override `CLEAR_ROUTE` | `O1NS` = dọn đường bắc–nam tại `I1` (do `L1` xử lý) |
| `D` | Dump trạng thái mạng lưới hiện tại ra màn hình/console | — |

Ghi chú: phím `X<crossing><hướng>` (tàu thoát) ở bản nháp trước đã được bỏ khỏi bộ input Core, vì việc mở lại gate giờ dùng timer (Mục 14) thay vì dựa vào exit sensor; phím này có thể được thêm lại nếu sau này exit sensor được nối vào 1 tính năng mở rộng.

Console sẽ hiển thị rõ ràng kết quả chuyển trạng thái sau mỗi lần bấm phím, vì phần đánh giá demo yêu cầu hành vi có thể thấy được, chứng minh được, không chỉ là 1 phím bấm được chấp nhận.

---

[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · [← An toàn và real-time](04-an-toan-va-real-time.md) · Tiếp theo: [Giả định và tổng kết →](06-gia-dinh-va-tong-ket.md)
