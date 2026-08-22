[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · Chương 1/6 · Tiếp theo: [Mạng lưới và kiến trúc →](02-mang-luoi-va-kien-truc.md)

---

## 1. Tầm nhìn dự án (Project Vision)

Chúng ta đang xây một **hệ thống điều khiển giao thông và chắn đường sắt phân tán, an toàn khi lỗi (fail-safe)**, chứ không phải một bảng đèn được điều khiển tập trung. Đặc điểm cốt lõi của hệ thống: **quyền điều khiển output vật lý luôn nằm ở local** — chín local controller gồm 6 local controller giao lộ `L1–L6` (mỗi cái sở hữu giao lộ vật lý `I1–I6` của nó) và 3 local controller đường sắt `RL1–RL3` (mỗi cái sở hữu chỗ giao vật lý `RC1–RC3` của nó) — mỗi controller sở hữu 1 tập thiết bị vật lý riêng và tiếp tục hoạt động an toàn, độc lập với phần còn lại của mạng lưới. Node thứ 10, **Central Controller `C1`**, hoàn toàn không có quyền actuate (tác động vật lý) — nó chỉ tồn tại để *quan sát*, *phối hợp*, *cấu hình lại trong giới hạn an toàn*, và *override trong giới hạn không bao giờ được vi phạm safety invariant ở local*.

Điều làm dự án này mạnh với môn EEET2588 không phải là số lượng giao lộ mô phỏng, mà là việc nó đưa ra câu trả lời thật sự, có thể demo được cho các câu hỏi cốt lõi của real-time systems: chuyện gì xảy ra khi 1 message đến trễ, khi 1 đường truyền rớt, khi 2 sự kiện an toàn cùng tranh chấp 1 actuator, và khi 1 component chết giữa chừng 1 chuỗi xử lý quan trọng. Chỗ giao đường sắt cố ý được chọn làm trung tâm của lập luận về an toàn (một interlock an toàn tính mạng thật sự: gate-confirmed-closed trước khi cho train-proceed), còn 6 giao lộ là phương tiện để chứng minh phối hợp phân tán (offset kiểu green-wave), quyền tự chủ local, và abstraction lệnh từ trung tâm.

Thiết kế cố ý loại phần *mô phỏng giao thông* (mô hình hoá xe đến, nhu cầu thống kê) ra khỏi phạm vi. Đây là dự án **hệ thống điều khiển**: các *sự kiện* từ sensor (mô phỏng bằng phím bấm, theo [REQ]) mới là thứ hệ thống phản ứng, chứ không phải một mô hình luồng giao thông cần kiểm chứng riêng.

---

## 2. Nguồn gốc yêu cầu

| Nguồn | Vai trò |
|---|---|
| `RTS_Final Project.pdf` | Nguồn chính thức. Định nghĩa: 6 giao lộ, 5 con đường, hành lang đường sắt đôi chạy chéo, boom gate + đèn nhấp nháy đỏ, khoảng cách giữa các chuyến tàu, nguyên tắc local tự điều khiển đèn của mình, Central chỉ giám sát/không actuate, 3 nhóm chuỗi vận hành (fixed / sensor / advanced), khả năng override, yêu cầu multi-QNX-node, phạm vi Pass tối thiểu (I1–I2 + crossing), và khuyến khích dựa thiết kế trên 1 giao lộ thật. |
| `lecture_clarification.md` | Bổ sung mang tính chính thức. Làm rõ: tổng cộng 9 local controller (6 giao lộ + 3 đường sắt, không phải "cùng loại"), local controller đường sắt độc quyền sở hữu boom gate/flasher/train-signal, local controller giao lộ chỉ được *sense* trạng thái đường sắt, yêu cầu train signal rõ ràng kèm báo lỗi, luồng lệnh Central→Local (không bao giờ Central→đèn trực tiếp), sensor mô phỏng bằng phím bấm kèm bảng phím rõ ràng, phối hợp kiểu green-wave dựa trên khoảng cách, khái niệm giảm ùn tắc do đường sắt, và sự linh hoạt giữa QNX node và máy vật lý (1 máy có thể chạy nhiều node logic; hướng dẫn chung của lớp là khoảng 2–3 máy vật lý cho 1 buổi demo đầy đủ). |
| `idea.md` | **Không phải là thiết kế.** Đây là tài liệu brainstorm/liệt kê câu hỏi. Chỉ dùng để xác định câu hỏi nào *quan trọng, ảnh hưởng lớn* (được trả lời rõ ràng bên dưới) so với câu hỏi ít giá trị/quá sớm (được giải quyết ngầm bằng 1 mặc định hợp lý, không bàn lại từng cái). |
| `README.md` | Bản nháp sớm. Diagram kiến trúc (`L1–L6` / `I1–I6`, `RL1–RL3` / `RC1–RC3`, `C1`) và cấu trúc thư mục của nó khớp với các quyết định bên dưới và được giữ lại; các tuyên bố về phạm vi của nó bị thay thế bởi tài liệu này. |

---

## 3. Mục tiêu thiết kế

1. **An toàn là cấu trúc, không phải ngẫu nhiên.** Mọi tình huống nguy hiểm (2 hướng cùng xanh, tàu chạy khi gate mở, người đi bộ đang băng qua) đều được ngăn chặn bởi 1 invariant thực thi *ở local*, không bao giờ dựa vào việc tin rằng 1 message từ xa sẽ đến đúng và đúng lúc.
2. **Quyền tự chủ local là thật, không phải hình thức.** Mọi local controller phải chứng minh được khả năng vận hành an toàn đầy đủ khi `C1` bị tắt.
3. **Quyền của Central chỉ là giám sát + phối hợp + override có giới hạn**, không bao giờ actuate trực tiếp. Sự khác biệt này phải thể hiện rõ trong thiết kế IPC, không chỉ nói suông.
4. **Độ phức tạp phải có lý do.** Mọi tính năng vượt trên mức Pass tối thiểu phải gắn với 1 điểm dạy học cụ thể của real-time systems (xử lý lỗi, phối hợp, phân tích timing, vận hành ở chế độ suy giảm) — tính năng chỉ để tăng diện tích bề mặt bị loại bỏ rõ ràng (Mục 23).
5. **Thiết kế phải mở rộng bằng cách nhân bản, không phải viết lại.** Bộ controller Pass-minimum (`L1`, `L2` tại `I1`, `I2`, và `RL1` tại `RC1`) phải chạy *cùng 1* binary controller tổng quát như `L3–L6` / `RL2–RL3`, chỉ khác nhau ở cấu hình — đây chính là ý của "the complete design should duplicate this part" [REQ].
6. **Mọi thứ tuyên bố phải demo được.** Không tính năng nào được đưa vào nếu không thể trigger trực tiếp và quan sát được trong 15 phút demo [REQ].

---

## 4. Phạm vi (Scope)

### 4.1 Hệ thống đầy đủ theo khái niệm (Full Conceptual System)

Thiết kế đầy đủ bao phủ toàn bộ mạng lưới: 5 con đường (`R1–R5`), 6 giao lộ có đèn tín hiệu (`I1–I6`) mỗi cái có 1 local controller riêng (`L1–L6`), 3 chỗ giao đường sắt (`RC1–RC3`) mỗi cái có 1 local controller riêng (`RL1–RL3`), và 1 Central Controller (`C1`) — tổng cộng 10 logical controller [REQ][CLARIF]. Mọi nhóm tính năng mô tả trong tài liệu này (chuỗi fixed/sensor/advanced, xử lý người đi bộ, vòng đời đường sắt đầy đủ kể cả lỗi, giảm ùn tắc, phối hợp/green-wave, override, vận hành ở chế độ suy giảm) đều thuộc thiết kế đầy đủ theo khái niệm và được phản ánh trong Initial Design Report, state chart, task architecture — bất kể có thật sự implement bằng code hay không.

### 4.2 Proof of Concept dự kiến

**Core PoC (chắc chắn làm, đạt mức Pass tối thiểu [REQ]):**
Giao lộ `I1`/`I2` (controller `L1`, `L2`), đường nối `R3`, chỗ giao đường sắt `RC1` (controller `RL1`), và `C1`. Đây là 1 "lát cắt 1/3" hoạt động đầy đủ — QNX process thật, IPC thật, interlock đường sắt thật, hành vi degraded-mode thật.

**Mở rộng HD-Target — đã chốt [TEAM, chốt ngày 2026-08-22]:**
Team nhắm tới HD, không phải mức Pass, nên phần mở rộng này giờ là 1 phần của kế hoạch build, không còn là "làm nếu còn thời gian". Nhân bản lại lát cắt Core một lần nữa dọc theo 1 trục arterial để thêm `I3`/`I4` (controller `L3`, `L4`), đường `R4`, và chỗ giao đường sắt `RC2` (controller `RL2`) — chọn cụ thể vì chỉ 1 lát cắt đơn lẻ thì không thể demo phối hợp trên trục arterial (offset kiểu green-wave cần ít nhất 2 giao lộ nối tiếp trên cùng 1 con đường, ví dụ `I1–I3` trên `R1`). Phần mở rộng này chính là thứ nâng demo từ "1 cặp giao lộ chạy được" lên thành "1 mạng lưới controller phối hợp phân tán thật sự" — đây mới là điểm khác biệt tạo ra HD.

**Bản build đầy đủ 9 controller (`I5`/`I6` qua `L5`/`L6`, `R5`, `RC3` qua `RL3`):** Tuỳ chọn/stretch — xem mục 4.3. Vì phần mềm controller mang tính tổng quát, cấu hình hoá (Mục tiêu 5, Mục 3), nên *thiết kế* đã bao phủ sẵn; việc có thật sự dựng thêm lần thứ 3 lúc demo hay không là quyết định về nguồn lực, không phải lỗ hổng thiết kế.

### 4.3 Phạm vi tuỳ chọn (Optional Scope)

Thiết kế làn rẽ riêng + đèn mũi tên protected (xem phân tích Mục 5b), giao diện đồ hoạ (GUI) ngoài terminal text [REQ cho phép chỉ dùng terminal], mô hình hoá xe đến theo thống kê/ngẫu nhiên, ghi log dữ liệu bền vững ngoài phần dùng `/fs` cho test-vector đã được phép [REQ], và lát cắt mạng lưới thứ 3 (`L5/L6` tại `I5/I6`, `RL3` tại `RC3`) là tuỳ chọn/stretch — xem Mục 23 để biết phân loại và lý do đầy đủ.

---

[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · Tiếp theo: [Mạng lưới và kiến trúc →](02-mang-luoi-va-kien-truc.md)
