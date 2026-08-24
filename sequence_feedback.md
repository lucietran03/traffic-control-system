SEQUENCE_DIAGRAMS.md

Lỗi cú pháp: không có — bản trước khi sửa và bản sau khi sửa đều render sạch bằng mermaid-cli thật (đã test cả 2, cả 8 diagram).

Lỗi logic thật sự (đã sửa):
- SD-07 (nặng nhất, 3 lỗi cùng lúc):
  1. `ACK` được gửi trước khi kiểm tra xong pedestrian clearance — sai thứ tự. Đúng ra phải đợi clearance hoàn tất + re-validate request rồi mới ACK (UC-08 Alt 3.1, và khớp với state chart SC-03B đã có state OVERRIDE_PENDING). Đã chuyển việc chờ clearance lên trước khối alt ACK/NACK.
  2. Thiếu hẳn nhánh NACK khi pedestrian clearance đang chạy mà không thể defer được (UC-08 Alt 3.1 có ghi rõ "...or is rejected if it cannot be safely deferred"). Đã thêm nhánh này.
  3. Thiếu nhánh gia hạn (renew) override — PROJECT_SPECIFICATION.md Mục 18 nói rõ "auto-expiring... if not renewed" nên renewal là cơ chế có thật, và SC-03B (state chart) đã có self-loop renew rồi, riêng sequence diagram này thiếu, gây lệch 2 file companion. Đã thêm loop renew.
  4. Thiếu note traceability gap PA-11 (brief gốc bắt buộc phải nêu rõ, không được im lặng bỏ qua vì system_assumptions_tables.md chưa có ID này). Đã thêm note.

- SD-04: thiếu nhánh lỗi khi gate không xác nhận mở lại được (GATE_OPEN_CONFIRMED fail) — trong khi SC-04B (state chart) có nhánh OPENING --> FAULT tương ứng, nên 2 file bị lệch nhau. Đã thêm alt nhánh fault khi mở lại.

Lỗi "thiếu note/nhánh bắt buộc theo use case gốc" (không phải tự ý thêm):
- SD-01: thiếu note anti-starvation (DP-06) ở cuối.
- SD-02: nhánh "stuck-active button" bị đặt tuốt ở cuối, sau khi pedestrian đã qua đường và request đã clear xong — vô lý về mặt thời gian, đúng ra phải xảy ra ngay lúc bấm nút (UC-02 Alt 2.2). Đã dời lên đúng chỗ. Cũng thiếu hẳn nhánh Central override đến giữa lúc clearance (UC-02 Alt 5.1) — đã thêm lại.
- SD-03: thiếu 2 alt flow mà UC-03 nêu rõ (profile thiếu/cũ → chạy standalone không chờ C1; railway pre-emption làm gián đoạn → phục hồi qua profile hợp lệ kế tiếp, không có state resync riêng). Đã thêm lại 2 opt block. Cũng bị mất số liệu cụ thể 21s/45s (Section 20's worked example) khi gộp 3 controller thành 1 loop chung — đã thêm note giữ lại số liệu.
- SD-05: message QUEUE_WARNING báo cáo giữa lúc đóng crossing bị "treo" không dẫn tới hành động nào — đã thêm note liên kết với CC-01 (feed vào logic suppression) cho rõ mục đích.
- SD-08: thiếu opt "railway event xảy ra lúc mất kết nối Central, chứng minh zero Central dependency" — brief gốc có yêu cầu rõ. Đã thêm lại, và đặt tên rõ giai đoạn RESYNCHRONISING cho khớp với SC-05.

Không có lỗi: SD-06.

Toàn bộ những chỗ thiếu này đến từ lần viết lại "gọn cho report" — logic tổng thể không sai bản chất, chỉ là một số nhánh/nốt bắt buộc theo đúng use case và assumption table bị lược mất, và SD-07 bị tái lỗi giống lỗi đã sửa ở lượt trước.
