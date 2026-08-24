STATE_CHARTS.md
Lỗi cú pháp: không có — bản cũ và bản mới đều render sạch bằng mermaid-cli thật (đã test cả 2).

Lỗi logic thật sự (đã sửa):
- SC-03: thiếu hẳn nhánh gia hạn override (operator renews). Đây không phải mình bịa — PROJECT_SPECIFICATION.md Mục 18 nói rõ "auto-expiring... if not renewed", tức renewal là cơ chế có thật, và SEQUENCE_DIAGRAMS.md (SD-07) đã có nó rồi — chỉ riêng state chart này thiếu, gây lệch giữa 2 file companion. Đã thêm self-loop ACTIVE --> ACTIVE.

Lỗi "thiếu note bắt buộc" (theo đúng yêu cầu gốc, không phải tự ý thêm):
- SC-02: thiếu note về (a) request giữ nguyên khi bị railway pre-emption chặn (PA-02), (b) nút bấm im lặng vĩnh viễn không phát hiện được (PA-03) — cái này brief gốc ghi rõ "must be shown only as a note".
- SC-04: thiếu note về (a) con số ~45s là tổng ngân sách, không phải thời lượng 1 state, (b) sensor tiếp cận tàu im lặng vĩnh viễn không phát hiện được (RC-11) — cũng là "must be shown as a note" trong brief gốc.
- SC-05: thiếu note "mất kết nối Central không tự động ép all-red/FLASHING_RED".

Toàn bộ những note này đã bị lược mất trong lần "làm gọn report" trước, mình khôi phục lại đúng nội dung bắt buộc.

Cấu trúc tách A/B/C đã áp dụng:
- SC-01 → SC-01A (chọn mode, sơ đồ tổng quan 2 hộp đen) + SC-01B (chi tiết PEAK_FIXED) + SC-01C (chi tiết OFF_PEAK_SENSOR).
- SC-03 → SC-03A (4 trạng thái quyền hạn tổng quan) + SC-03B (chi tiết validate/queue/renew của override).
- SC-04 → SC-04A (approach → đóng gate) + SC-04B (tàu chiếm dụng → mở lại), nối nhau qua trạng thái CLOSED.
- SC-02, SC-05 giữ nguyên 1 hình vì đã đủ đơn giản.

SEQUENCE_DIAGRAMS.md