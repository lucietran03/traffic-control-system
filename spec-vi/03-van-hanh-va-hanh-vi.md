[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · Chương 3/6 · [← Mạng lưới và kiến trúc](02-mang-luoi-va-kien-truc.md) · Tiếp theo: [An toàn và real-time →](04-an-toan-va-real-time.md)

---

## 11. Chế độ vận hành (Operating Modes)

**Nói đơn giản trước, bảng chi tiết ở dưới:**
- `PEAK_FIXED` = giờ cao điểm — đèn chạy theo lịch cố định, không đợi xe.
- `OFF_PEAK_SENSOR` = giờ vắng — đèn chờ có xe/người đi bộ mới bật xanh.
- `RAILWAY_PREEMPTION` = có tàu sắp tới — hướng xe đi về phía đường ray phải chuyển đỏ sớm hơn bình thường, bất kể đang ở 2 mode trên.
- `CENTRAL_OVERRIDE` = operator ở trung tâm ra lệnh đặc biệt (ví dụ dọn đường cho đoàn xe ưu tiên).
- `DEGRADED_LOCAL` = mất liên lạc với trung tâm — đèn vẫn tự chạy an toàn theo cấu hình gần nhất đã biết.
- `FAULT_SAFE` = phát hiện lỗi — đèn chuyển về trạng thái an toàn nhất (nhấp nháy đỏ) và ngừng phản ứng bình thường.

| Mode | Mục đích | Điều kiện vào | Hành vi | Lệnh Central được phép | Điều kiện thoát | Độ ưu tiên |
|---|---|---|---|---|---|---|
| `PEAK_FIXED` | Timing dễ dự đoán, dễ phối hợp khi nhu cầu cao | Theo giờ trong ngày hoặc `SET_MODE` | Chu kỳ 2 pha cố định, chia theo ưu tiên arterial; sensor chỉ được theo dõi cho mục đích ùn tắc/lỗi, không dùng để gia hạn | `SET_TIMING_PROFILE`, `SET_MODE`, override | `SET_MODE` hoặc đổi theo giờ trong ngày | Bình thường (7) |
| `OFF_PEAK_SENSOR` | Phản ứng theo nhu cầu khi lưu lượng thấp | Theo giờ trong ngày hoặc `SET_MODE` | Chọn/gia hạn pha dựa trên sensor phát hiện xe + yêu cầu người đi bộ đã latch, có giới hạn min/max green | `SET_TIMING_PROFILE`, `SET_MODE`, override | `SET_MODE` hoặc đổi theo giờ trong ngày | Bình thường (7) |
| `RAILWAY_PREEMPTION` | Bảo vệ chỗ giao đường sắt | `RLx` báo `WARNING`/`CLOSING`/`CLOSED` cho crossing liền kề | Chồng lên mode hiện tại của `Lx`: ép hướng đi về phía crossing chuyển đỏ qua clearance an toàn, tạm ngưng chọn pha bình thường cho approach đó, ưu tiên các hướng "drain" (thoát hàng chờ) sau khi mở lại | Không lệnh nào được chấp nhận nếu làm hướng về crossing xanh trở lại | Crossing báo `OPEN` và pha drain hoàn tất | 2 |
| `CENTRAL_OVERRIDE` | Tình huống đặc biệt (ví dụ dọn đường cho đoàn xe VIP) [REQ] | `REQUEST_OVERRIDE` được chấp nhận | Ép 1 hướng đi chỉ định thành xanh trong thời lượng giới hạn | (chính nó là lệnh) | Hết thời lượng hoặc bị huỷ tường minh | 4 |
| `DEGRADED_LOCAL` | Tiếp tục an toàn khi không có Central | Heartbeat từ `C1` timeout | Tiếp tục mode/profile đã biết gần nhất, tự chủ hoàn toàn; tự chọn Peak/Off-Peak theo đồng hồ local | Không (link đang mất) | Kết nối phục hồi + local đẩy toàn bộ trạng thái hiện tại lên `C1` | Song song (không nằm trong hierarchy nguy hiểm — đây là trạng thái sẵn sàng, không phải hazard) |
| `FAULT_SAFE` | Ngăn chặn 1 hazard đã phát hiện | Xung đột phần cứng local, watchdog trip, hoặc (với `RLx`) trạng thái gate chưa xác nhận khi có rủi ro tàu | Ép output về trạng thái an toàn (Mục 10), ngừng phản ứng với lệnh không liên quan an toàn | Không lệnh nào được chấp nhận trừ lệnh xoá lỗi (có thể cần thao tác của operator) | Xoá tường minh, không bao giờ tự động | 1 (cao nhất) |

---

## 12. Hành vi giao thông bình thường

Mỗi giao lộ được mô hình hoá như 1 giao lộ 4 nhánh, 2 pha chuẩn (arterial vs. connector), không phân pha theo từng làn (mô hình 2 làn permissive của Core — xem Mục 5b nếu team muốn xem phương án phân pha theo làn rẽ riêng):

- **Pha A** — arterial (`R1`/`R2`) đi thẳng+trái xanh, connector đỏ.
- Vàng (4 giây **[ASSUM]**) → All-red clearance (2 giây **[ASSUM]**).
- **Pha B** — connector (`R3`/`R4`/`R5`) đi thẳng+trái xanh, arterial đỏ; WALK người đi bộ tương thích chạy đồng thời.
- Vàng → All-red clearance → lặp lại.

`PEAK_FIXED`: tổng chu kỳ ≈ 90 giây **[ASSUM]**, chia ưu tiên nghiêng về arterial (ví dụ ~55 giây Pha A / ~35 giây Pha B trước khi trừ vàng/clearance), khớp với ưu tiên cao hơn đã gán cho arterial (Mục 5). Sensor vẫn được theo dõi nhưng không làm thay đổi timing ở mode này, giữ nguyên tính xác định (deterministic) mà phối hợp/green-wave cần.

`OFF_PEAK_SENSOR`: 1 pha chỉ nhận xanh khi (các) approach của nó báo có nhu cầu hoặc đang giữ 1 yêu cầu người đi bộ đã latch; xanh được gia hạn theo bước 4 giây **[ASSUM]** tới trần 40 giây **[ASSUM]** khi nhu cầu còn tồn tại; khi không có nhu cầu ở bất kỳ đâu, giao lộ nghỉ ở pha arterial (mặc định ưu tiên đường chính) [REQ — "differ between R1-R5, teams must state and justify"]. 1 đường phụ không bao giờ phải chờ lâu hơn 1 lần gia hạn tối đa của pha đối diện cộng min green của chính nó — đây là 1 giới hạn chống đói (anti-starvation) mang tính cấu trúc, không phải 1 thuật toán trọng số.

Việc đổi mode (`PEAK_FIXED ↔ OFF_PEAK_SENSOR`, dù do đồng hồ local hay do Central ra lệnh) chỉ được áp dụng tại ranh giới pha tiếp theo, không bao giờ giữa chừng 1 pha **[TEAM]** — điều này bảo toàn mọi invariant về clearance/min-green bất kể yêu cầu đổi mode đến lúc nào.

---

## 13. Hành vi người đi bộ

- 4 lối qua đường mỗi giao lộ (Bắc/Đông/Nam/Tây), 1 nút bấm mỗi bên [REQ].
- 1 lần bấm nút sẽ **latch** thành 1 yêu cầu đang chờ cho bên đó; bấm lặp lại khi đã latch không có tác dụng thêm (bấm 10 lần = 1 yêu cầu) **[TEAM]**.
- WALK cho người đi bộ chạy **đồng thời** với pha xe không xung đột với lối qua đó (kiểu parallel-walk phổ biến tại các giao lộ có đèn tín hiệu ở Việt Nam), không phải 1 pha all-red riêng kiểu "scramble" — pha scramble đã được cân nhắc và loại bỏ vì thêm độ phức tạp không có cơ sở (Mục 23).
- Chuỗi tín hiệu: `WALK → FLASHING_DONT_WALK (clearance) → DONT_WALK` **[TEAM]** — khớp với mô hình đèn người đi bộ 3 trạng thái phổ biến, không phải tín hiệu 2 trạng thái đơn giản.
- Được phục vụ ở cả `PEAK_FIXED` (đảm bảo mỗi chu kỳ, vì cả 2 pha đều chạy mỗi chu kỳ) và `OFF_PEAK_SENSOR` (1 yêu cầu đã latch được xử lý y như nhu cầu của xe khi lên lịch).
- Khi WALK đã bắt đầu, nó phải chạy hết chu trình clearance trước khi lối qua đó có thể bị gián đoạn bởi bất kỳ thứ gì, kể cả 1 override từ Central (Safety Invariant, Mục 22).
- 1 yêu cầu người đi bộ đến trong lúc railway pre-emption đang diễn ra sẽ được **latch, không bị bỏ**. Vì pre-emption chỉ ép *hướng đi về phía crossing* thành đỏ (Mục 15) và lối qua đường cho người đi bộ chỉ tồn tại ở giao lộ, không tồn tại tại chỗ giao đường sắt **[TEAM — ghi chú phạm vi: lối qua người đi bộ nằm ngoài phạm vi tại `RCx`, vì đề bài chỉ đặt chúng "at each intersection"]**, yêu cầu này thường vẫn được phục vụ bình thường ở pha không xung đột.
- 1 nút bấm không bao giờ nhả (kẹt) hoặc không phản hồi được xử lý fail-safe như **1 yêu cầu đang chờ thường trực** (giả định luôn có nhu cầu thật) thay vì bị bỏ qua, và 1 lỗi được báo về `C1` **[ASSUM]**.

---

## 14. Hành vi đường sắt

Toàn bộ vòng đời bình thường, thuộc quyền sở hữu hoàn toàn của `RLx` [CLARIF]:

1. **Phát hiện tàu** — sensor tiếp cận kích hoạt → `RLx` lập tức bật đèn nhấp nháy và lập tức thông báo cho 2 `Lx` liền kề và `C1` (trạng thái → `WARNING`).
2. **Bắt đầu dọn giao thông** — `Lx` liền kề bắt đầu chuyển an toàn sang đỏ cho hướng đi về phía crossing (hoàn tất clearance đang diễn ra nếu có; không cắt xanh đột ngột — Mục 22).
3. **Gate bắt đầu đóng** — sau khoảng cảnh báo chỉ-nhấp-nháy ban đầu, `RLx` ra lệnh hạ cả 2 cánh gate (trạng thái → `CLOSING`).
4. **Gate xác nhận đã đóng** — sensor vị trí xác nhận cả 2 cánh đã hạ (trạng thái → `CLOSED`).
5. **Đặt train signal** — `PROCEED` chỉ được cấp **khi** gate đã xác nhận `CLOSED`; nếu không thì vẫn giữ `STOP` và báo lỗi (Safety Invariant).
6. **Bắt đầu cửa sổ chiếm dụng (occupancy window)** — ngay khi sensor tiếp cận kích hoạt, `RLx` cũng tính toán 1 **cửa sổ chiếm dụng crossing** cố định cho đoàn tàu đó: `thời điểm dự kiến đến + khoảng thời gian cố định để qua hết crossing`. Đây là cách thay thế bằng timer cho việc phát hiện qua exit sensor (Mục 9, mục 5) — giả định **[ASSUM]** là mọi đoàn tàu đều mất cùng 1 khoảng thời gian đã biết để qua hết crossing kể từ lúc tới, nên 1 timer là đủ và không cần exit sensor cho quyết định này.
7. **Tàu đang qua** — trạng thái là `TRAIN_PRESENT` chừng nào *bất kỳ* cửa sổ chiếm dụng nào (bước 6) của bất kỳ hướng nào chưa hết hạn.
8. **Gate mở lại** — chỉ khi **mọi** cửa sổ chiếm dụng đang hoạt động (cả 2 hướng, kể cả 1 đoàn tàu thứ hai đến gần ngay sau) đã hết hạn thì `RLx` mới nâng gate (trạng thái → `OPENING → OPEN`), tắt đèn nhấp nháy, và broadcast `OPEN`. Nếu 1 đoàn tàu thứ hai được phát hiện trước khi cửa sổ của đoàn tàu đầu hết hạn, gate đơn giản là vẫn giữ nguyên — cửa sổ chiếm dụng của nó được cộng thêm vào tập cửa sổ phải hết hạn hết trước khi mở lại (xử lý tự nhiên trường hợp 2 tàu đến gần nhau, cùng hướng hoặc ngược hướng, mà không cần logic đặc biệt riêng).
9. **Giao thông trở lại bình thường** — `Lx` liền kề tiếp tục pha gần nhất trước đó (không reset hoàn toàn) và chạy 1 pha drain kéo dài trên đường connector để giải phóng hàng xe đã tích luỹ trong lúc đóng (Mục 16) trước khi quay lại chu kỳ bình thường.

**Phương pháp tính timing [ASSUM, Proposed — Awaiting Confirmation]:** đề bài không cung cấp số liệu về thời gian dẫn trước (lead time), nên các giá trị được *suy ra*, không phải bịa đặt:
- **Thời gian dẫn trước từ cảnh báo tới lúc tàu đến** ≈ cảnh báo chỉ-nhấp-nháy (5 giây) + thời gian gate đóng (10 giây) + biên an toàn xác nhận đã đóng (5 giây) + biên bổ sung cho chuỗi clearance an toàn riêng của giao lộ liền kề (~25 giây) ⇒ mục tiêu **~45 giây từ lúc phát hiện tàu tới lúc tàu tới crossing**. Khoảng cách đặt sensor tiếp cận được suy ra từ `tốc độ tàu giả định tối đa × 45 giây` (ví dụ 80 km/h ≈ 22 m/s ⇒ ≈1000 m).
- **Khoảng thời gian chiếm dụng crossing** (bước 6 ở trên) ≈ 1 giá trị cố định giả định, ví dụ **20 giây**, đại diện cho chiều dài tàu + bề rộng crossing ở tốc độ tàu giả định, giống nhau cho mọi đoàn tàu mô phỏng.

Cả 2 giá trị đều được trình bày rõ trong báo cáo như tham số suy ra, có thể điều chỉnh, không phải sự thật bịa đặt.

**Đánh đổi được chấp nhận:** dùng timer cố định thay vì sensor xác nhận thoát nghĩa là thiết kế hiện tại chưa phát hiện được trường hợp đặc biệt 1 đoàn tàu chạy chậm bất thường hoặc bị kẹt trên crossing. Đây là 1 sự đơn giản hoá có chủ đích, có ghi chép rõ ràng cho thiết kế Core — không phải 1 thiếu sót — và exit sensor được giữ lại (Mục 9, mục 5) chính là hướng đã xác định để lấp lỗ hổng này sau này nếu còn thời gian.

---

## 15. Railway Pre-emption và việc dọn giao thông

**Mô tả đơn giản:** "pre-emption" ở đây hoạt động giống việc xe cứu thương được ưu tiên tại 1 giao lộ — chỉ khác là thứ được bảo vệ là chỗ giao đường sắt, không phải 1 chiếc xe. Ngay khi `RLx` phát hiện 1 đoàn tàu đang tới, nó không đợi chu kỳ bình thường của giao lộ liền kề tới lượt pha connector; nó lập tức báo cho các `Lx` liền kề "có tàu sắp tới", và mỗi `Lx` sắp xếp lại ưu tiên để hướng đi cụ thể dẫn về crossing mất đèn xanh (qua chuỗi clearance an toàn bình thường, không đột ngột) từ trước khi gate cần hạ xuống khá lâu.

Pre-emption mang tính **theo hướng, không phải toàn giao lộ [TEAM]**: chỉ (các) hướng đi dẫn về crossing đang đóng/sắp đóng bị ép đỏ; giao thông cắt ngang và pha người đi bộ không xung đột với hướng đó vẫn được phục vụ bình thường bởi cùng `Lx`. Ví dụ, tại `I1` (liền kề `RC1` qua `R3`), chỉ hướng `R3` đi về phía crossing bị ảnh hưởng — pha arterial `R1` và bất kỳ lối qua người đi bộ nào không xung đột với hướng `R3` vẫn hoạt động theo lịch bình thường. Cách này vừa thực tế hơn vừa an toàn hơn khi implement so với việc đóng băng toàn bộ giao lộ, vì nó tránh phải bịa ra các trường hợp xung đột mới ngoài ma trận xung đột bình thường của giao lộ.

Việc chuyển sang đỏ của hướng đi về crossing luôn hoàn tất qua chuỗi vàng + all-red clearance bình thường (Safety Invariant) — pre-emption thay đổi *độ ưu tiên lên lịch*, không bao giờ *bỏ qua* chuỗi clearance, vì làm vậy sẽ tự tạo ra 1 hazard ngay tại giao lộ.

---

## 16. Quản lý ùn tắc do đường sắt

Theo giải thích của giảng viên, mục tiêu là ngừng *đẩy* giao thông về phía 1 crossing đang đóng, chứ không phải bịa ra khả năng đổi hướng giao thông mà hệ thống đèn không thể thật sự làm được. Cụ thể, hệ thống đèn giao thông chỉ có thể làm đúng 3 việc, không hơn — đặc tả này nói rõ điều đó thay vì ngụ ý hệ thống "giải quyết" được ùn tắc:

1. **Ngừng cấp nhu cầu về phía crossing.** Ngay khi pre-emption bắt đầu, hướng đi về crossing ngừng nhận xanh cho tới khi crossing mở lại (Mục 15) — đây là đòn bẩy chính.
2. **Ưu tiên các hướng đi ra khỏi crossing và giao thông cắt ngang** tại giao lộ liền kề trong lúc đóng, để các xe đã xếp hàng giữa giao lộ và crossing có cơ hội rẽ đi hướng khác thay vì tích tụ — chỉ khi hình học làn/rẽ hiện có cho phép; đặc tả không tuyên bố hệ thống có thể tạo ra 1 tuyến đường vật lý mới.
3. **Chạy 1 pha drain có giới hạn sau khi mở lại** — gia hạn xanh vượt trần max-green bình thường trên đường connector, cụ thể để giải phóng hàng xe tích luỹ trong lúc đóng, trước khi quay lại chu kỳ chuẩn.

Advance/queue sensor (Mục 9, mục 2) tồn tại cụ thể để phát hiện khi hàng xe trên approach hướng về crossing đang dài dần về phía crossing, nuôi cả logic ngừng cấp nhu cầu lúc pre-emption lẫn việc tính kích thước pha drain khi phục hồi.

---

[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · [← Mạng lưới và kiến trúc](02-mang-luoi-va-kien-truc.md) · Tiếp theo: [An toàn và real-time →](04-an-toan-va-real-time.md)
