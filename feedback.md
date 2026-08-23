Điểm 1 & 3 (actor cho lỗi nội bộ): Ý là — khi 1 con sensor báo lỗi hay 1 process bị crash, không có "ai" bên ngoài bấm nút hay tạo ra sự kiện đó cả (khác với người đi bộ bấm nút, hay tài xế lái xe tới). Nhưng CLAUDE.md bắt buộc mỗi use case phải có 1 Primary Actor. Nên mình đề xuất coi chính cái sensor/thiết bị đó là actor (vì nó là "input source" từ bên ngoài phần mềm, dù nó là phần cứng của hệ thống vật lý) — giống như cách "Vehicle" hay "Train" cũng không phải người, mà là nguồn sự kiện. Ví dụ: UC-04 (gate lỗi) → actor = "Boom-Gate Position Sensor" (chính con sensor đó là bên phát ra sự kiện lỗi). Bạn đồng ý hướng này, hay muốn để trống Primary Actor luôn?

Điểm 2 (UC-05 actor sai): Bản cũ ghi actor là "Local Hardware Clock" — cái này SAI vì đồng hồ đó nằm bên trong Lx, tức là 1 phần của chính hệ thống, không phải "bên ngoài". Mình đề xuất đổi actor thành Central Control Room Operator — không phải vì operator "gây ra" việc mất kết nối, mà vì họ là bên duy nhất liên quan bên ngoài (họ sẽ thấy giao lộ "biến mất" khỏi màn hình giám sát, và sau này họ là người mà hệ thống báo cáo lại khi kết nối lại).

---
Điểm 4 — giải thích lại từ đầu, dễ hiểu hơn:

Ý tưởng chính: mỗi "Business Rule" trong use case phải trỏ về 1 nguồn cụ thể để chứng minh không phải tự mình bịa ra luật đó. Hiện tại, cách mình đang làm là trỏ về 1 mã ID trong file system_assumptions_tables.md, ví dụ:

▎ BR-1: Train signal chỉ được PROCEED khi gate đã xác nhận đóng (RC-06)

Người đọc report thấy RC-06 thì lật qua bảng assumption, tìm đúng dòng RC-06, đọc được lý do đầy đủ.

Vấn đề với UC-07 (Central override): khi mình đi tìm trong bảng system_assumptions_tables.md xem có dòng nào mô tả đúng luật "override tối đa 5 phút, tự hết hạn, bị từ chối nếu có tàu sắp tới" — thì không tìm thấy dòng nào cả. Luật này chỉ được viết trong file khác (PROJECT_SPECIFICATION.md, Mục 18), không có trong bảng assumption.

Vậy giờ có 2 đường đi:

- Cách A — thêm 1 dòng mới vào bảng assumption trước (ví dụ đặt tên PA-11), viết rõ luật override vào đó, rồi UC-07 trỏ về PA-11 — giống y hệt cách mọi use case khác đang làm. Sạch sẽ, đồng bộ, nhưng phải sửa thêm 1 file.
- Cách B — không thêm gì, cứ để UC-07 ghi thẳng "xem Mục 18 của PROJECT_SPECIFICATION.md" thay vì trỏ về 1 mã ID. Nhanh hơn, nhưng riêng UC-07 sẽ trace kiểu khác với 6 use case còn lại (những cái kia trỏ về mã ID, cái này trỏ về số mục) — nhìn không đồng nhất khi giám khảo đọc cả report.

Mình nghiêng về Cách A để giữ mọi use case trace theo đúng 1 kiểu duy nhất. Bạn chọn A hay B?