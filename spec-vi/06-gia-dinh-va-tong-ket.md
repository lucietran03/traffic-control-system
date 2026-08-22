[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · Chương 6/6 · [← Tính năng và demo](05-tinh-nang-va-demo.md)

---

## 26. Giả định (Assumptions)

| # | Giả định | Lý do | Hệ quả thiết kế | Cần giảng viên xác nhận? |
|---|---|---|---|---|
| 1 | Đi bên phải (right-hand traffic, quy ước Việt Nam), làn sát dải phân cách = trái+thẳng, làn sát lề = phải+thẳng | Bối cảnh thực tế dự án là Việt Nam; brief gốc không bắt buộc chọn quốc gia/quy ước cụ thể, chỉ khuyến khích dựa trên 1 giao lộ thật | Xác định hướng vẽ Diagram 2, cách gán làn ở Mục 5 | Không |
| 2 | Tốc độ arterial 60 km/h, connector 50 km/h, khoảng cách 300–500 m | Giá trị đô thị Việt Nam tiêu biểu (phù hợp khung tốc độ QCVN 41:2019/BGTVT cho đường đô thị), chỉ dùng để *suy ra* phương pháp tính timing | Nuôi việc tính chu kỳ, tỷ lệ chia pha, offset phối hợp | Không |
| 3 | Min green 8 giây, max green 40 giây, yellow 4 giây, all-red 2 giây | Khoảng giá trị kỹ thuật đèn tín hiệu đô thị điển hình, không gắn với 1 tiêu chuẩn quốc gia cụ thể | Giới hạn mọi timer pha trong thiết kế | Không |
| 4 | Chu kỳ `PEAK_FIXED` ≈ 90 giây, chia ưu tiên arterial | Chu kỳ giao lộ cỡ trung tiêu biểu | Cơ sở cho timing fixed-mode và offset green-wave | Không |
| 5 | Thời gian dẫn trước cảnh báo đường sắt ≈ 45 giây, suy từ lead-flash + gate-close + biên an toàn | Đề bài không cho số; giá trị suy từ phương pháp an toàn hơn số bịa đặt | Xác định khoảng cách đặt sensor tiếp cận và ngân sách phản ứng pre-emption | Không — đánh dấu có thể điều chỉnh |
| 6 | Khoảng thời gian chiếm dụng crossing cố định ≈ 20 giây, giống nhau mọi đoàn tàu | Đơn giản hoá logic mở lại thành timer thay vì cần xác nhận exit sensor, theo quyết định team | Việc mở gate (Mục 14) dùng timer; exit sensor để dành, không phải Core | Không — đánh dấu có thể điều chỉnh; đánh đổi đã ghi chép (không phát hiện tàu chạy chậm/bị kẹt bất thường trong phạm vi Core) |
| 7 | Core dùng 2 làn/chiều, rẽ permissive, không đèn mũi tên (thay vì 3 làn protected) | Phân tích đầy đủ ở Mục 5b: rủi ro lịch trình + tăng chu kỳ (ảnh hưởng UX) của phương án 3 làn lớn hơn lợi ích an toàn tại điểm rẽ trái, xét trong bối cảnh phạm vi đánh giá của môn học | Actuator/sensor/conflict-matrix Core giữ đơn giản; phương án 3 làn ghi nhận là hướng nâng cấp tương lai, không phải Core | Không — quyết định nội bộ team |
| 8 | Hệ thống không mô phỏng xe cụ thể hay va chạm giữa các xe — chỉ mô phỏng sự kiện sensor (bấm phím) và trạng thái đèn | PoC chỉ hiển thị trạng thái đèn qua terminal [REQ], không có xe/driver được mô phỏng; rủi ro va chạm khi rẽ trái permissive phụ thuộc luật nhường đường ngoài đời thực, nằm ngoài phạm vi hệ thống có thể mô phỏng hoặc chịu trách nhiệm (xem Mục 5b) | Xác nhận Core giữ thiết kế 2 làn permissive; demo sẽ không bao giờ thể hiện "xe" hay "tai nạn", chỉ thể hiện trạng thái đèn/sensor hợp lệ | Không |
| 9 | Chỉ 2 khung nhu cầu, Peak/Off-Peak | Đủ để kiểm chứng mọi chuyển mode mà không cần thêm số liệu vô căn cứ | Đơn giản hoá `SET_MODE` và tự chọn theo giờ trong ngày | Không |
| 10 | Không mô hình hoá xe đến theo thống kê/ngẫu nhiên; sensor hoàn toàn theo sự kiện | Dự án là hệ thống điều khiển, không phải traffic simulator | Mô hình input sensor hoàn toàn dựa trên sự kiện phím bấm | Không |
| 11 | Heartbeat 1 giây, mất link sau 3 lần liên tiếp | Đủ nhanh cho demo đã rút gọn thời gian, theo mẫu missed-N chuẩn | Cơ sở cho thời điểm vào `DEGRADED_LOCAL` | Không |
| 12 | Override tối đa 5 phút, tự hết hạn | Tránh 1 override bị quên làm thay đổi vận hành vĩnh viễn | Giới hạn mode `CENTRAL_OVERRIDE` | Không |
| 13 | Lối qua người đi bộ chỉ tồn tại ở giao lộ, không tồn tại tại chỗ giao đường sắt | Đề bài chỉ đặt lối qua người đi bộ "at each intersection" | Đơn giản hoá phân tích tương tác pre-emption/người đi bộ | Không |
| 14 | Topology demo được tham số hoá kiểu "thực tế tổng quát" (lấy cảm hứng từ giao lộ Quận 2) thay vì map vào 1 giao lộ thật có tên cụ thể | Thoả ý định đề bài (căn timing theo nguyên tắc thực tế) mà không đổi topology đã cho | Né hoàn toàn điều kiện cần duyệt khi đổi topology | Không, theo thiết kế |
| 15 | Triển khai demo dùng 2 máy vật lý (Mục 27) | Ít điểm hỏng hơn về phần cứng/mạng khi demo trực tiếp, được đánh giá; QNX node mang tính logic và không phụ thuộc vị trí, nên 2 máy vẫn chạy được mọi process riêng biệt cần thiết | Diagram triển khai nhắm 2 máy; có thể mở rộng lên 3 sau này mà không cần đổi code | Không |

---

## 27. Quyết định thiết kế còn mở — đã chốt ngày 2026-08-22

Cả 3 mục dưới đây từng mở ở bản nháp trước, giờ team đã quyết định:

1. **Cam kết mở rộng HD cho PoC — ĐÃ CHỐT: có làm.** Team nhắm tới HD. Phần mở rộng `L3`/`L4`/`RL2` (Mục 4.2) là 1 phần của kế hoạch build, không phải mục tiêu stretch. Vậy nên phối hợp green-wave (Mục 20) là 1 tính năng *được demo thật*, không chỉ thiết kế trên giấy.
2. **Số máy vật lý cuối cùng — ĐÃ CHỐT: khuyến nghị 2 máy.** Lý do: 1 "node" QNX là khái niệm logic/phần mềm, không phải vật lý — message passing qua Qnet hoạt động y hệt dù 2 process nằm cùng máy hay khác máy. Vậy nên 2 máy vật lý vẫn có thể chạy mỗi controller process trên 1 QNX node riêng, thoả đầy đủ yêu cầu "multiple QNX nodes with separate processes" [REQ]. Máy thứ 3 chỉ thêm rủi ro thật sự vào ngày demo (thêm phần cứng, thêm cấu hình mạng, thêm 1 thứ có thể lỗi lúc khởi động/kết nối) mà không thêm khả năng nào mà yêu cầu thật sự cần. Nếu sau này team muốn 1 demo "rõ ràng đây là 2 hộp máy riêng biệt" thuyết phục hơn về mặt hình ảnh, chuyển sang 3 máy chỉ là thay đổi cấu hình triển khai, không phải thay đổi thiết kế.
3. **Watchdog/supervisor process trên mỗi node — ĐÃ CHỐT: có làm, vì team nhắm HD.** Mô tả đơn giản: đây là 1 process QNX rất nhỏ, phụ, chạy trên mỗi node, chỉ có 1 nhiệm vụ duy nhất — "kiểm tra xem controller process thật trên node này còn phản hồi không; nếu không, ép output của node đó về trạng thái an toàn đã định nghĩa (Mục 10) ngay lập tức." Nó rẻ để build, không tương tác gì với logic giao thông còn lại, và trực tiếp minh chứng 1 mẫu giám sát real-time thật sự (Mục 17, Mục 23). Việc xếp lịch cụ thể (làm ở sprint nào) do team quyết định.

**Quyết định bổ sung — chốt cùng đợt cập nhật diagram/lane:**

4. **Quy ước lái xe — ĐÃ CHỐT: Việt Nam, đi bên phải.** Xem Mục 5.
5. **Lựa chọn làn đường Core — ĐÃ CHỐT: 2 làn permissive, không đèn mũi tên.** Phương án 3 làn protected được ghi nhận làm hướng nâng cấp có thể cân nhắc trong tương lai (Mục 5b) — không phải cho môn học này.
6. **PoC không mô phỏng xe/va chạm xe — ĐÃ CHỐT.** Hệ thống chỉ mô phỏng sự kiện sensor và trạng thái đèn; không có "xe" hay "tai nạn" nào được demo (xem Giả định #8, Mục 26).

---

## 28. Câu hỏi cho Team / Giảng viên

**Trả lời ngắn gọn: hiện tại không có gì trong đặc tả này cần giảng viên duyệt.** Điều kiện duy nhất giảng viên nêu ra để cần xin duyệt là *thay đổi bản đồ/topology đã cho* (ví dụ đổi số làn hoặc bố cục giao lộ). Thiết kế này không làm vậy — Mục 5 chỉ đặt tên và định hướng cho các con đường vốn đã ngụ ý trong diagram chính thức và gợi ý mức Pass tối thiểu; việc chọn quy ước đi bên phải và tham chiếu thực tế Việt Nam cũng không phải thay đổi topology. Mọi giá trị timing số trong Mục 26 đều được trình bày như giả định kỹ thuật có thể suy ra/điều chỉnh, không phải tuyên bố yêu cầu, nên cũng không cần duyệt.

Điều còn lại duy nhất cần "xác nhận với ai đó" chỉ là sở thích của team, không phải câu hỏi cho giảng viên: nếu sau này team muốn báo cáo nhắc đến 1 giao lộ thật cụ thể có tên tại Việt Nam (thay vì tham số hoá thực tế tổng quát ở Giả định 13), đó chỉ là 1 quyết định nội bộ ngắn gọn, không cần email hỏi giảng viên.

---

## 29. Truy vết nguồn gốc (Traceability)

| Tính năng | Nguồn gốc |
|---|---|
| 6 giao lộ, 5 con đường, hành lang đường sắt, boom gate/flasher, local tự điều khiển đèn, Central chỉ giám sát, 3 nhóm chuỗi vận hành, override, multi-node QNX, lát cắt Pass-minimum | [REQ] |
| Tổng 9 local controller, `RLx` độc quyền sở hữu thiết bị đường sắt, yêu cầu train signal + báo lỗi, kỳ vọng bảng phím mô phỏng, hướng dẫn triển khai 2–3 máy | [CLARIF] |
| Tách tên `Lx`/`Ix` và `RLx`/`RCx`, cách map topology theo tên đường (Mục 5), quy ước đi bên phải, quyết định làn Core 2-làn-permissive (Mục 5b), mô hình giao lộ 2 pha, mẫu parallel-walk cho người đi bộ, hierarchy ưu tiên, chi tiết command abstraction, hướng resync ở degraded-mode, quyết định dùng timer cho crossing-occupancy, danh sách tính năng bị loại | [TEAM] |
| Mọi giá trị timing số, khoảng cách đặt sensor, số liệu heartbeat/timeout, khoảng thời gian chiếm dụng crossing cố định | [ASSUM] |
| Advance/queue sensor + ngừng cấp nhu cầu ùn tắc, phối hợp green-wave, watchdog/supervisor process, override tự hết hạn có giới hạn, local từ chối lệnh không an toàn | [HD] |

---

## 30. Tóm tắt hệ thống đề xuất cuối cùng

Hệ thống là 1 mạng lưới điều khiển giao thông và đường sắt phân tán, đặt an toàn lên hàng đầu: 6 local controller giao lộ (`L1–L6`, mỗi cái ứng với 1 giao lộ vật lý `I1–I6`) và 3 local controller đường sắt (`RL1–RL3`, mỗi cái ứng với 1 chỗ giao vật lý `RC1–RC3`) đều giữ quyền độc quyền, có thể hoạt động offline đối với thiết bị vật lý của riêng mình, được phối hợp — chứ không bao giờ bị ra lệnh ở mức đèn — bởi 1 Central Controller (`C1`) chỉ có thể giám sát, cấu hình lại trong giới hạn an toàn, và phát override có giới hạn mà local controller có quyền từ chối. Mạng lưới lấy bối cảnh Việt Nam (đi bên phải), với topology `R1`/`R2` (arterial, Tây–Đông) và `R3`/`R4`/`R5` (connector, Bắc–Nam, cắt hành lang đường sắt tại `RC1`/`RC2`/`RC3`). Chỗ giao đường sắt là trung tâm của lập luận an toàn (gate-confirmed-closed trước khi train-proceed, 1 timer cửa sổ chiếm dụng cố định chi phối việc mở lại an toàn, fail-to-safe với mọi sự mơ hồ), còn mạng lưới 6 giao lộ là trung tâm của lập luận phối hợp/phân tán (phân pha local 2 pha với làn permissive, hành vi off-peak theo sensor, offset green-wave trên arterial, và cơ chế chống đói mang tính cấu trúc). Proof-of-concept chắc chắn thực hiện là bộ controller Pass-minimum `L1`/`L2`/`RL1` (tại `I1`/`I2`/`RC1`) cộng `C1`, được xây bằng cùng 1 phần mềm controller tổng quát, cấu hình hoá — nhân bản được thật sự (không chỉ hứa suông) lên toàn mạng lưới 6 giao lộ, 3 crossing. Team cam kết thực hiện phần mở rộng HD-target (`L3`/`L4`/`RL2` tại `I3`/`I4`/`RC2`), chính là thứ khiến phối hợp đa giao lộ và ngừng cấp nhu cầu ùn tắc trở thành demo được thật sự chứ không chỉ là lý thuyết, triển khai trên 2 máy vật lý để giữ rủi ro ngày demo ở mức thấp trong khi vẫn thoả yêu cầu multi-QNX-node. Thiết kế làn rẽ riêng + đèn mũi tên protected (Mục 5b) được nghiên cứu và tài liệu hoá đầy đủ như 1 phương án nâng cấp có cơ sở, dành cho `I1`/`I2` nếu team còn dư thời gian sau khi hoàn thành phạm vi HD đã cam kết.

---

[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · [← Tính năng và demo](05-tinh-nang-va-demo.md)
