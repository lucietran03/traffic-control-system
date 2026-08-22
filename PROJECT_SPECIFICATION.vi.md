# Hệ Thống Điều Khiển Đèn Giao Thông — Đặc Tả Dự Án (Mục Lục)

**Bản dịch tiếng Việt của `PROJECT_SPECIFICATION.md`.** Đây là **file làm việc chính** của team (theo thoả thuận: team đọc/sửa trực tiếp bản tiếng Việt; khi có thay đổi, bản tiếng Việt sẽ được dịch ngược lại và ghi đè lên `PROJECT_SPECIFICATION.md` tiếng Anh — bản tiếng Anh là bản chính thức để nộp, nhưng không cần chỉnh sửa song song).

Tên component, ký hiệu trạng thái, tên lệnh và các thuật ngữ kỹ thuật (`L1–L6`, `I1–I6`, `RC1–RC3`, `RLx`, `C1`, `PEAK_FIXED`, `GREEN`, v.v.) được **giữ nguyên tiếng Anh** để khớp với code/diagram/UML sau này.

Vì file gốc quá dài, đặc tả được chia thành các chương nhỏ trong thư mục [`spec-vi/`](spec-vi/). File này chỉ là **mục lục + tài liệu tham chiếu dùng chung** (bảng nhãn nguồn gốc, quy ước đặt tên).

---

## Mục lục

| Chương | Nội dung | Mục số |
|---|---|---|
| [`spec-vi/01-tam-nhin-va-pham-vi.md`](spec-vi/01-tam-nhin-va-pham-vi.md) | Tầm nhìn dự án, nguồn gốc yêu cầu, mục tiêu thiết kế, phạm vi (Core PoC / HD-target / tuỳ chọn) | Mục 1–4 |
| [`spec-vi/02-mang-luoi-va-kien-truc.md`](spec-vi/02-mang-luoi-va-kien-truc.md) | Mạng lưới vật lý (bao gồm phân tích lựa chọn làn đường 5b), thành phần hệ thống, kiến trúc controller, trách nhiệm từng controller, sensor, actuator | Mục 5–10 |
| [`spec-vi/03-van-hanh-va-hanh-vi.md`](spec-vi/03-van-hanh-va-hanh-vi.md) | Chế độ vận hành, hành vi giao thông/người đi bộ/đường sắt, railway pre-emption, quản lý ùn tắc | Mục 11–16 |
| [`spec-vi/04-an-toan-va-real-time.md`](spec-vi/04-an-toan-va-real-time.md) | Hành vi lỗi/an toàn, hành vi Central, quyền tự chủ local, chiến lược timing/phối hợp, yêu cầu real-time, safety invariant | Mục 17–22 |
| [`spec-vi/05-tinh-nang-va-demo.md`](spec-vi/05-tinh-nang-va-demo.md) | Phân loại tính năng, kịch bản demo, input mô phỏng | Mục 23–25 |
| [`spec-vi/06-gia-dinh-va-tong-ket.md`](spec-vi/06-gia-dinh-va-tong-ket.md) | Giả định, quyết định còn mở (đã chốt), câu hỏi cho team/giảng viên, truy vết nguồn gốc, tóm tắt cuối cùng | Mục 26–30 |

Sơ đồ minh hoạ (ASCII, tiếng Việt): [`SYSTEM_DIAGRAMS.vi.md`](SYSTEM_DIAGRAMS.vi.md).

---

## Nhãn nguồn gốc (dùng xuyên suốt mọi chương)

| Nhãn | Ý nghĩa |
|---|---|
| **[REQ]** | Yêu cầu chính thức — nêu trực tiếp trong `RTS_Final Project.pdf` |
| **[CLARIF]** | Giải thích của giảng viên — nêu trong `lecture_clarification.md` |
| **[TEAM]** | Quyết định của team — chốt cứng để giải quyết 1 điểm mà đề bài để mở |
| **[ASSUM]** | Giả định kỹ thuật — 1 giá trị/hành vi được đặt ra để thiết kế cụ thể hơn, có lý do, và có thể điều chỉnh |
| **[HD]** | Đề xuất tính năng nâng cao (HD) — không bắt buộc, thêm vào để làm mạnh câu chuyện real-time/distributed systems |
| **[CONFIRM]** | Cần giảng viên xác nhận — thật sự còn mơ hồ so với tài liệu chính thức |

Nếu 1 quyết định là do team tự cân nhắc (chứ không phải yêu cầu cứng), nó sẽ được đánh thêm **Proposed — Awaiting Confirmation** để team có thể đổi lại mà không cần lục lại lý do.

---

## Quy ước đặt tên — đọc trước tiên

Nguồn gốc gây nhầm lẫn lớn nhất ở bản nháp trước là lẫn lộn giữa **vị trí vật lý** và **controller** quản lý nó. Toàn bộ đặc tả tách bạch rõ ràng:

| Thứ vật lý (hạ tầng) | Controller sở hữu nó (logic/phần mềm) |
|---|---|
| `I1`–`I6` — 6 giao lộ có đèn tín hiệu vật lý | `L1`–`L6` — 6 **local controller** của giao lộ (`Lx` điều khiển `Ix`) |
| `RC1`–`RC3` — 3 chỗ giao đường sắt vật lý | `RL1`–`RL3` — 3 **local controller** đường sắt (`RLx` điều khiển `RCx`) |
| `R1`–`R5` — 5 con đường vật lý | *(không có controller — đường không bị điều khiển)* |
| Phòng điều khiển trung tâm | `C1` — **Central Controller** |

Vậy: "`I1`" luôn có nghĩa là *giao lộ như một địa điểm*; "`L1`" luôn có nghĩa là *phần mềm/process điều khiển đèn tại địa điểm đó*. Cách này khớp với diagram kiến trúc gốc trong `README.md` (đã dùng đúng `L1–L6` từ đầu).

## Quy ước hướng đi đường bộ

Team đặt bối cảnh thực tế của dự án ở **Việt Nam** (đi bên phải — right-hand traffic), tham chiếu ví dụ giao lộ có đèn mũi tên tại Quận 2 (TP.HCM) thay vì ví dụ Melbourne trong brief gốc. Chi tiết và hệ quả thiết kế ở [`spec-vi/02-mang-luoi-va-kien-truc.md`](spec-vi/02-mang-luoi-va-kien-truc.md), Mục 5 và 5b.
