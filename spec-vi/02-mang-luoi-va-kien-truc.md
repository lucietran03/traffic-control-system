[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · Chương 2/6 · [← Tầm nhìn và phạm vi](01-tam-nhin-va-pham-vi.md) · Tiếp theo: [Vận hành và hành vi →](03-van-hanh-va-hanh-vi.md)

---

## 5. Mạng lưới vật lý (Physical Network)

Diagram chính thức [REQ] cho thấy 1 hành lang đường sắt đôi chạy chéo cắt qua lưới giao lộ 3×2. Diễn giải diagram đó cùng với gợi ý mức Pass tối thiểu ("2 intersections I1–I2 and the railway crossing") và giải thích đặt tên của giảng viên [CLARIF] cho ra topology cụ thể sau — **[TEAM]**, suy trực tiếp từ diagram gốc, không phải thay đổi topology, nên không cần giảng viên duyệt:

- **`R1`** — trục arterial hướng Tây–Đông, đi qua `I1 → I3 → I5` (hàng phía Bắc).
- **`R2`** — trục arterial hướng Tây–Đông, đi qua `I2 → I4 → I6` (hàng phía Nam).
- **`R3`** — đường nối theo hướng Bắc–Nam `I1 ↔ I2`, cắt đường sắt tại **`RC1`**.
- **`R4`** — đường nối theo hướng Bắc–Nam `I3 ↔ I4`, cắt đường sắt tại **`RC2`**.
- **`R5`** — đường nối theo hướng Bắc–Nam `I5 ↔ I6`, cắt đường sắt tại **`RC3`**.
- Hành lang đường sắt đôi chạy ngang theo hướng Tây–Đông, cắt qua cả 3 đường nối `R3`/`R4`/`R5` tại `RC1`/`RC2`/`RC3`.

Xem `SYSTEM_DIAGRAMS.vi.md`, **Diagram 1**, để có bản hình ảnh đầy đủ của mạng lưới này (đã xoay theo đúng hướng team yêu cầu: `R1`/`R2` nằm ngang Tây–Đông, `R3`/`R4`/`R5` nằm dọc Bắc–Nam, đi từ Bắc xuống Nam sẽ lần lượt gặp cặp `I1`/`I2`, rồi `I3`/`I4`, rồi `I5`/`I6`).

**Nói đơn giản, "arterial" và "connector" chỉ là 2 tên gọi cho 2 vai trò khác nhau, không phải thuật ngữ gì phức tạp:**
- **`R1`, `R2` = arterial = 2 con đường DÀI.** Mỗi đường chạy xuyên suốt, nối liền 3 giao lộ cùng hàng (`R1` nối `I1-I3-I5`, `R2` nối `I2-I4-I6`). Đây là 2 trục chính, xe chạy thẳng qua nhiều giao lộ.
- **`R3`, `R4`, `R5` = connector = 3 con đường NGẮN.** Mỗi đường chỉ nối đúng 1 cặp giao lộ (`R3` nối `I1-I2`, `R4` nối `I3-I4`, `R5` nối `I5-I6`). Đây cũng là 3 con đường DUY NHẤT cắt qua đường ray.

Vậy phân loại này không dựa trên "hướng" (cả 2 loại đều có hướng riêng theo Diagram 1), mà dựa trên **vai trò**: dài/xuyên suốt (arterial) so với ngắn/chỉ nối 1 cặp + có đi qua đường ray (connector).

Cách đặt tên này cho đúng 5 con đường và 6 giao lộ [REQ], biến lát cắt Pass-minimum (`I1–I2–RC1`) thành đúng 1/3 lặp lại của toàn mạng lưới (thoả "the complete design should duplicate this part" [REQ]), và tách bạch rõ **arterial** (ưu tiên xe chạy thẳng qua, ứng viên cho phối hợp green-wave) khỏi **connector** (lưu lượng cơ bản thấp hơn, nhưng lại là những con đường trực tiếp chịu ảnh hưởng của railway pre-emption và rủi ro ùn tắc) — trả lời trực tiếp cho câu F5 trong `idea.md` về sự khác biệt giữa traffic priority và coordination sensitivity.

**Quy ước lái xe [TEAM, chốt ngày 2026-08-22]:** dự án lấy bối cảnh **Việt Nam, đi bên phải (right-hand traffic)** theo Điều 10 Luật Trật tự, an toàn giao thông đường bộ số 36/2024/QH15. Ví dụ bối cảnh cụ thể là giao lộ có đèn tín hiệu Mai Chí Thọ–Đồng Văn Cống tại khu vực Quận 2 cũ, với layout được ghi nhận trong H. D. Nguyen, *Capacity Analysis of Signalised Intersections in Motorcycle Dependent Cities*, Hình A-7. Đây chỉ là reference cho bối cảnh thực tế, không phải tuyên bố topology hoặc timing của project được đo tại site này. Với đi bên phải, dải phân cách nằm bên trái và lề đường/vỉa hè nằm bên phải theo hướng di chuyển.

Mọi con đường đều 2 chiều với 2 làn mỗi chiều [REQ]. Cách chia làn Core **[TEAM]**: làn sát dải phân cách = đi thẳng + rẽ trái (permissive, nhường xe ngược chiều), làn sát lề đường = đi thẳng + rẽ phải (permissive); không có đèn mũi tên rẽ riêng trong thiết kế Core. Xem `SYSTEM_DIAGRAMS.vi.md`, **Diagram 2**, để biết bố cục làn/hướng đi theo đúng quy ước đi bên phải; xem **Mục 5b** ngay dưới đây cho phân tích đầy đủ và phương án thay thế (làn riêng + đèn mũi tên protected) đã được cân nhắc kỹ nhưng không đưa vào Core.

Tỷ lệ các hướng rẽ và đường phụ/lối vào được xác định rõ **ngoài phạm vi [ASSUM]**: đề bài cho phép loại bỏ đường phụ vì không quan trọng, và 1 dự án về *logic điều khiển tín hiệu* không cần mô hình nhu cầu giao thông để có tính thuyết phục — các *sự kiện* từ sensor (không phải xe mô phỏng) mới là thứ điều khiển mọi hành vi phản ứng.

**Giả định về khoảng cách và tốc độ [ASSUM, Proposed — Awaiting Confirmation]:** thay vì map lên 1 giao lộ thật có tên cụ thể, mạng lưới dùng các khoảng cách arterial minh hoạ `I1–I3=350 m`, `I3–I5=400 m`, `I2–I4=320 m`, `I4–I6=380 m` và tốc độ arterial giả định 60 km/h (16,67 m/s). Tốc độ này phù hợp mức tối đa hiện hành cho loại đường đô thị đủ điều kiện theo Thông tư 38/2024/TT-BGTVT, Điều 6, Bảng 1. Các giá trị đều là tham số có thể điều chỉnh, dùng để tính offset ở Mục 20 chứ không được trình bày như số đo khảo sát thực địa.

---

## 5b. Phân tích: làn dùng chung + rẽ permissive so với làn riêng + đèn mũi tên protected

Team đặt câu hỏi: nếu làn trong vừa cho đi thẳng vừa cho rẽ trái (permissive), và chiều đối diện cũng đang xanh để đi thẳng, thì 2 xe có "đụng nhau" không? Câu trả lời kỹ thuật: **không tự động đụng nhau**, nhưng đây đúng là 1 đánh đổi an toàn có thật, đáng phân tích kỹ trước khi quyết định — dưới đây là nghiên cứu đầy đủ theo yêu cầu của team, không chỉ xét khía cạnh kỹ thuật mà cả vận hành, timing, và trải nghiệm người dùng.

### Phương án A — 2 làn, rẽ permissive, 1 đèn tròn/approach (thiết kế Core hiện tại)

**Cách hoạt động:** trong pha xanh của 1 hướng (ví dụ arterial), CẢ 2 chiều đối diện đều được xanh đồng thời (đi thẳng + rẽ trái permissive + rẽ phải permissive). Xe rẽ trái phải tự nhường xe đi thẳng ngược chiều, giống hệt cách rất nhiều giao lộ nhỏ/vừa ở Việt Nam vận hành khi không có đèn mũi tên riêng (tài xế tự phán đoán khoảng trống để rẽ). Đây **không phải lỗi thiết kế gây tai nạn ngay lập tức** — nó là 1 mô hình vận hành tiêu chuẩn, hợp lệ, được dùng rộng rãi trên thế giới; rủi ro an toàn phụ thuộc vào phán đoán của tài xế, không phải vào logic đèn bị sai.

**Ưu điểm:**
- Chu kỳ đèn ngắn hơn nhiều: chỉ 2 pha/giao lộ (arterial vs. connector) → tổng delay trung bình cho MỌI người dùng đường thấp hơn.
- Ít actuator/sensor/state hơn hẳn → giảm rủi ro implement sai trong thời gian PoC có hạn, dồn được thời gian cho các tính năng HD đã cam kết (green-wave, congestion, watchdog, multi-node).
- Ma trận xung đột (Mục 22) đơn giản, dễ chứng minh đúng — quan trọng vì đây là dự án được đánh giá về đúng đắn an toàn, không phải về mô phỏng giao thông chân thực nhất có thể.

**Nhược điểm:** rủi ro an toàn tại điểm rẽ trái cao hơn phương án B, vì phụ thuộc vào tài xế thay vì được đèn bảo vệ tuyệt đối.

### Phương án B — 3 làn riêng biệt (trái/thẳng/phải), 3 đèn mũi tên, rẽ trái protected

**Cách hoạt động:** mỗi approach có làn rẽ trái riêng (chỉ rẽ trái), làn đi thẳng riêng, làn rẽ phải riêng — mỗi làn có đèn mũi tên riêng. Rẽ trái CHỈ được phép khi đèn mũi tên trái xanh, lúc đó chiều đối diện đang đỏ (protected) — loại bỏ hoàn toàn điểm xung đột permissive-left. Đây là alternative protected-turn tổng quát, không được trình bày như một đặc điểm đã đo tại giao lộ contextual reference.

**Ưu điểm:** loại bỏ hẳn rủi ro va chạm tại điểm rẽ trái vì không còn phụ thuộc phán đoán tài xế; trực quan, dễ hiểu cho người lái.

**Nhược điểm (đây là phần team yêu cầu nghiên cứu kỹ):**
1. **Timing/trải nghiệm người dùng xấu đi cho tất cả mọi người.** Rẽ trái protected cần 1 pha riêng trong chu kỳ (không thể chạy chung với chiều đối diện). Từ 2 pha/giao lộ (arterial, connector) tăng lên tối thiểu 4 pha (arterial thẳng+phải, arterial rẽ trái riêng, connector thẳng+phải, connector rẽ trái riêng) — mỗi pha thêm lại cần thêm yellow + all-red clearance riêng. Kết quả: chu kỳ dài hơn đáng kể, tăng thời gian chờ trung bình cho **mọi** người dùng đường, kể cả người không hề rẽ trái. Đây là đánh đổi trực tiếp "an toàn hơn cho 1 nhóm nhỏ" đổi lấy "chậm hơn cho tất cả" — đúng loại phân tích timing/UX mà team muốn cân nhắc.
2. **Tăng mạnh actuator và độ phức tạp state machine.** Từ 1 đèn tròn/approach lên 3 đèn mũi tên/approach × 4 approach × (5–9 giao lộ tuỳ phạm vi) — tăng số state, tăng số trường hợp phải chứng minh an toàn trong ma trận xung đột (Mục 22), tăng rủi ro có bug sống sót tới lúc demo.
3. **Cần sensor riêng theo từng làn** nếu muốn `OFF_PEAK_SENSOR` phản ứng đúng (ví dụ không bật pha rẽ trái nếu không có xe nào chờ rẽ trái) — điều này đảo ngược 1 quyết định đã chốt có lý do rõ ràng ở Mục 23 (Rejected: "Đèn tín hiệu/sensor riêng theo từng làn").
4. **Rủi ro lịch trình demo.** Team đã cam kết 1 phạm vi HD khá tham vọng (green-wave + congestion management + watchdog + multi-node + railway interlock). Thêm 1 mảng tính năng lớn nữa (protected-turn phasing) làm tăng nguy cơ bị hỏi xoáy lúc Q&A ở đúng phần mới thêm, và giảm thời gian để làm chắc các phần đã cam kết — 1 rủi ro quản lý dự án thật sự, không chỉ kỹ thuật.
5. **Cần làn chờ đủ dài (storage length)** cho làn rẽ trái để không chặn làn đi thẳng phía sau — đây là bài toán hình học/queue mà Mục 5 đã chủ động đưa ra ngoài phạm vi từ đầu, vì dự án là hệ thống điều khiển, không phải mô phỏng capacity/hình học đường.
6. **Rủi ro "yellow trap"** — 1 vấn đề kỹ thuật giao thông có thật: nếu trộn permissive và protected không cẩn thận (ví dụ pha rẽ trái permissive kết thúc trong khi chiều đối diện vẫn còn xanh dài hơn), tài xế có thể bị đánh lừa tưởng an toàn để rẽ mà thực ra không phải. Muốn an toàn tuyệt đối phải làm **protected-only** (rẽ trái CHỈ được phép ở pha mũi tên, không bao giờ được phép lúc đèn tròn xanh) — giảm tính linh hoạt, có thể khiến giao lộ "cứng" hơn khi lưu lượng rẽ trái thấp.

### Vì sao permissive-left không phải lỗ hổng an toàn — và vì sao "tai nạn" không áp dụng cho PoC này

Điểm mấu chốt sau khi thảo luận kỹ với team: **PoC này không mô phỏng xe cụ thể hay va chạm giữa các xe** — hệ thống chỉ mô phỏng sự kiện sensor (bấm phím) và hiển thị trạng thái đèn, đúng theo brief ("sufficient to display the state... by text messages"). Vì vậy câu hỏi "có tai nạn không" không áp dụng theo nghĩa đen cho demo — không có 2 object xe nào tồn tại để va chạm.

Về mặt lý thuyết vận hành (nếu triển khai thật ngoài đời), cần phân biệt 2 loại xung đột khác nhau về bản chất:

| Loại xung đột | Ví dụ | Ai phân xử quyền đi trước? | An toàn theo định nghĩa? |
|---|---|---|---|
| Không xác định (prohibited) | 2 con đường khác nhau cùng được xanh | Không có luật nào — đèn tự mâu thuẫn | **Không** — đây chính là thứ Safety Invariant #1 (Mục 22) phải ngăn tuyệt đối |
| Đã có luật phân xử (permitted) | 1 chiều đi thẳng + chiều đối diện rẽ trái permissive | Luật giao thông: rẽ trái phải nhường xe đi thẳng ngược chiều | **Có** — cách vận hành tiêu chuẩn ở hầu hết giao lộ trên thế giới |

Permissive-left thuộc loại thứ 2: có luật phân xử rõ ràng, chỉ là luật đó được thực thi bởi con người thay vì bởi đèn — đây là rủi ro đã được luật hoá từ trước khi có đèn giao thông, không phải lỗ hổng do hệ thống đèn tạo ra.

### Khuyến nghị và quyết định cuối cùng **[TEAM, chốt]**

**Core giữ nguyên Phương án A (2 làn, permissive).** Bổ sung 1 giả định chính thức vào Mục 26: hệ thống không mô phỏng và không chịu trách nhiệm về va chạm xe-với-xe, vì không có xe nào được mô phỏng — chỉ có sự kiện sensor và trạng thái đèn. Phương án B (3 làn protected) được ghi nhận như 1 hướng nâng cấp có thể cân nhắc **trong tương lai, không phải bây giờ** — không cần thêm diagram riêng cho nó ở giai đoạn này.

Nếu sau này team muốn quay lại Phương án B (ví dụ khi mở rộng ngoài phạm vi môn học), bước hợp lý là nâng cấp trước 1–2 giao lộ gần đường sắt nhất (`I1`/`I2`), vì đó là nơi rẽ trái tương tác trực tiếp với railway pre-emption nên có giá trị an toàn cao nhất — xem Mục 23, Tuỳ chọn/Stretch.

---

## 6. Thành phần hệ thống

**Hạ tầng vật lý:** 5 con đường, 6 giao lộ có đèn tín hiệu, 3 chỗ giao đường sắt (đường đôi, mỗi hướng 1 line), 24 đầu đèn tín hiệu xe (4 approach × 6 giao lộ), 24 lối qua đường cho người đi bộ với 24 nút bấm + 24 đèn tín hiệu người đi bộ, 6 boom gate (2 mỗi crossing), 6 bộ đèn nhấp nháy hướng ra đường (2 mỗi crossing), 6 train signal (2 mỗi crossing, 1 mỗi hướng ray).

**Logical controller:** `L1–L6` (local controller giao lộ, mỗi cái ứng với 1 giao lộ vật lý `I1–I6`), `RL1–RL3` (local controller đường sắt, mỗi cái ứng với 1 chỗ giao vật lý `RC1–RC3`), `C1` (Central Controller). Tổng cộng 10 logical controller [CLARIF].

**Kiến trúc phần mềm/QNX:** mỗi logical controller là 1 hoặc nhiều QNX **process**; các controller được phân bổ trên nhiều QNX **node**; các node được phân bổ trên 1 số ít máy vật lý/ảo lúc demo (Mục 18). Việc ánh xạ 1:1:1 (controller = node = máy) rõ ràng **không** được giả định [CLARIF].

**Môi trường demo:** hiển thị trạng thái dạng terminal cho mỗi controller/console [REQ — không cần GUI], nhập liệu mô phỏng sensor/operator bằng bàn phím [REQ], tuỳ chọn ghi log test-vector qua `/fs` [REQ].

---

## 7. Kiến trúc controller

```text
                                CENTRAL CONTROLLER (C1)
                          monitor · reconfigure · override
                       (không bao giờ actuate thiết bị vật lý)
                    ________________|________________
                   |                                  |
        INTERSECTION CONTROLLERS               RAILWAY CROSSING CONTROLLERS
             L1 L2 L3 L4 L5 L6                       RL1  RL2  RL3
              |  |  |  |  |  |                         |    |    |
             I1 I2 I3 I4 I5 I6                       RC1  RC2  RC3
      (giao lộ vật lý)                        (chỗ giao đường sắt vật lý)

     Thiết bị mỗi Lx sở hữu:                   Thiết bị mỗi RLx sở hữu:
       - Đầu đèn tín hiệu xe                     - Boom gate
       - Đèn tín hiệu người đi bộ                 - Đèn nhấp nháy hướng ra đường
       - Sensor phát hiện xe                      - Train signal
       - Nút bấm người đi bộ                      - Sensor tiếp cận/thoát tàu
                   ^                                       |
                   |________ trạng thái đường sắt (chỉ sense) ___|
```

Quy tắc kiến trúc cốt lõi [CLARIF]: mũi tên từ `RLx` đến `Lx` chỉ mang **trạng thái** (crossing state), không bao giờ mang lệnh, và chỉ chạy 1 chiều. `Lx` không thể truy vấn hay tác động đến thiết bị đường sắt; nó chỉ có thể *biết* trạng thái hiện tại của crossing và phản ứng ở phần của nó trong interlock.

---

## 8. Trách nhiệm của từng controller

### 8.1 Central Controller (`C1`)

- **Input:** báo cáo trạng thái từ cả 9 local controller (trạng thái đèn, hàng đợi người đi bộ, trạng thái sensor, mode, lỗi), heartbeat.
- **Output/lệnh:** tới `Lx`: `SET_MODE(PEAK_FIXED | OFF_PEAK_SENSOR)`, `SET_TIMING_PROFILE(tham số trong giới hạn đã validate)`, `REQUEST_OVERRIDE(loại, mục tiêu, thời lượng)`; tới `RLx`: chỉ những yêu cầu cấp cao đã được xác thực (ví dụ xác nhận/xoá 1 lỗi đã báo cáo sau khi sửa vật lý xong — xem ghi chú đối chiếu bên dưới).
- **Trách nhiệm giám sát:** tổng hợp và hiển thị trạng thái đèn/người đi bộ/đường sắt toàn mạng lưới, duy trì log lỗi, phát hiện controller không phản hồi/mất kết nối qua heartbeat timeout.
- **Giới hạn rõ ràng [REQ][CLARIF]:** không thể đặt 1 đèn về 1 màu cụ thể; không thể trực tiếp actuate bất kỳ thiết bị đường sắt nào (boom gate, đèn nhấp nháy, train signal); không thể ép 1 lệnh vượt qua safety invariant mà local controller đã từ chối; không thể mở sớm 1 pedestrian clearance đang latch.
- **Đối chiếu câu chữ trong đề bài về đường sắt [REQ][CLARIF]:** đề bài viết operator "will occasionally send commands to the intersections, boom gate control and train approach signals system", trong khi cũng nói rõ Central không bao giờ trực tiếp điều khiển đèn tại 1 giao lộ, và lecture clarification nói chỉ `RLx` được actuate thiết bị đường sắt. 2 câu này **không mâu thuẫn** nếu đọc đúng: "lệnh tới boom gate control" của `C1` nghĩa là 1 lệnh cấp cao, đã xác thực, gửi tới `RLx` — controller sở hữu thiết bị đó — đúng theo command abstraction đã dùng cho `Lx` (không bao giờ là lệnh actuate thô gửi thẳng xuống phần cứng). `RLx` tự validate và quyết định có thực hiện hay không, có quyền từ chối/NACK (Mục 18).

### 8.2 Local controller giao lộ (`Lx`, điều khiển giao lộ `Ix`)

- **Thiết bị điều khiển:** 4 đầu đèn tín hiệu xe, 4 đèn tín hiệu người đi bộ (tại `Ix`).
- **Sensor local:** 4 sensor phát hiện xe theo approach, 4 nút bấm người đi bộ, và (HD, chỉ ở 2 giao lộ hướng về crossing của mỗi railway crossing) 1 advance/queue sensor.
- **Logic giao thông:** chạy 1 trong 2 mode `PEAK_FIXED` / `OFF_PEAK_SENSOR`, thực thi ma trận xung đột 2 pha, khoảng clearance, min/max green.
- **Logic người đi bộ:** latch nút bấm, phục vụ WALK trong pha xe tương thích, chạy đầy đủ chuỗi WALK → FLASHING-DON'T-WALK → DON'T WALK.
- **Thông tin đường sắt tiêu thụ:** trạng thái crossing (`OPEN/WARNING/CLOSING/CLOSED/TRAIN_PRESENT/OPENING/FAULT`) cho crossing liên quan về mặt địa lý (chỉ 2 controller nằm trực tiếp trên đường nối cắt qua tuyến đường sắt đó mới nhận — ví dụ chỉ `L1`/`L2` nhận trạng thái của `RC1`).
- **Hành vi fallback:** tiếp tục chạy đầy đủ logic local (kể cả sensor-driven) hoàn toàn không cần link tới Central; fallback về mode tự chọn theo giờ trong ngày.
- **Tương tác với Central:** nhận `SET_MODE`/`SET_TIMING_PROFILE`/override, chỉ áp dụng tại ranh giới pha an toàn tiếp theo, từ chối và NACK bất kỳ lệnh nào vi phạm safety invariant.

### 8.3 Local controller đường sắt (`RLx`, điều khiển crossing `RCx`)

- **Phát hiện tàu:** 2 sensor tiếp cận (1 mỗi hướng ray). Xem Mục 9 về tình trạng của exit sensor.
- **Điều khiển boom gate:** sở hữu độc quyền cả 2 cánh gate của crossing.
- **Điều khiển đèn nhấp nháy:** sở hữu độc quyền cả 2 bộ đèn nhấp nháy hướng ra đường.
- **Train signal:** sở hữu độc quyền cả 2 train signal (1 mỗi hướng); thực thi invariant gate-confirmed-closed-before-proceed.
- **State machine của crossing:** `OPEN → WARNING → CLOSING → CLOSED → (TRAIN_PRESENT) → OPENING → OPEN`, với trạng thái `FAULT` có thể vào từ bất kỳ điểm nào.
- **Xử lý lỗi:** fail-to-safe với bất kỳ trạng thái sensor/gate nào mơ hồ (Mục 17).
- **Thông tin chia sẻ cho controller đường bộ:** trạng thái crossing hiện tại, broadcast chỉ tới 2 `Lx` liền kề trên đường nối của nó.
- **Báo cáo về Central:** mọi lần chuyển trạng thái và mọi lỗi, ngay lập tức.

Cả 3 `RLx` chạy logic *giống hệt nhau* — đề bài không có lý do gì để chúng khác nhau, và việc bịa ra hành vi khác nhau giữa các crossing chỉ làm tăng diện tích cần test mà không có giá trị giảng dạy nào **[TEAM — từ chối phân biệt]**.

---

## 9. Sensor

| # | Sensor | Vị trí | Chủ sở hữu | Sự kiện | Mục đích / Mô phỏng |
|---|---|---|---|---|---|
| 1 | Sensor phát hiện xe theo approach | Tại vạch dừng, 1 mỗi approach (4 mỗi giao lộ) | `Lx` | `DEMAND_PRESENT` / `DEMAND_ABSENT` | Tương đương vòng cảm ứng (inductive-loop) tiêu chuẩn: "hiện có xe đang chờ ở approach này không?" Phím bấm cho mỗi approach. |
| 2 | Advance/queue sensor **[HD]** | Cách vạch dừng khoảng 60–80 m về phía upstream, mỗi railway crossing có 1 sensor trên từng approach giao lộ hướng về crossing | `Lx` | `QUEUE_WARNING` | Hình dung 2 vạch trên một approach: vạch A ngay tại vạch dừng (sensor #1), vạch B cách đó khoảng 70 m về phía upstream (sensor #2). Sensor #1 trả lời "có xe đang chờ không?"; sensor #2 trả lời "hàng xe đã kéo dài tới vạch B chưa?". Sensor #2 chỉ phục vụ logic giảm ùn tắc ở Mục 16, không dùng để gia hạn pha bình thường. Đây là sensor nhị phân, không đếm xe. Phím bấm riêng. |
| 3 | Nút bấm người đi bộ | 1 mỗi bên lối qua đường (4 mỗi giao lộ) | `Lx` | `PED_REQUEST(bên)` | Yêu cầu trực tiếp từ [REQ]. Phím bấm. |
| 4 | Sensor tiếp cận tàu (train approach sensor) | Khoảng cách cố định trước crossing, 1 mỗi hướng ray (2 mỗi crossing) | `RLx` | `TRAIN_APPROACHING(hướng)` | Yêu cầu trực tiếp từ [REQ] — đây là sự kiện khởi động toàn bộ chuỗi an toàn đường sắt (Mục 14). Phím bấm. |
| 5 | Sensor thoát tàu/xác nhận đã qua (exit/clearance sensor) — **để dành, không dùng trong logic Core [TEAM, đã sửa]** | Ngay sau crossing, 1 mỗi hướng (2 mỗi crossing) | `RLx` | *(không được nối vào quyết định mở gate ở Core)* | Ban đầu dự định để xác nhận chính xác thời điểm tàu đã qua khỏi crossing. **Đã đổi:** thiết kế Core giờ giả định mọi đoàn tàu đều mất cùng 1 khoảng thời gian cố định, đã biết trước để qua hết crossing kể từ lúc tới (Mục 14), nên việc mở lại gate dùng timer, không dùng sensor này. Sensor vẫn được giữ trong danh mục thiết bị vật lý như 1 chỗ dành sẵn cho **tính năng mở rộng trong tương lai** — ví dụ đối chiếu xem tàu có thật sự thoát khỏi crossing trong khoảng thời gian giả định hay không, để bắt các trường hợp tàu chạy chậm bất thường hoặc bị kẹt — nhưng việc kiểm tra đó rõ ràng nằm ngoài phạm vi logic an toàn Core hiện tại. |
| 6 | Sensor vị trí boom gate | Trên mỗi cánh gate (2 mỗi crossing) | `RLx` | `GATE_CLOSED_CONFIRMED` / `GATE_OPEN_CONFIRMED` / `GATE_FAULT` | Sensor này **không quan tâm mất bao lâu để đóng/mở** — nó chỉ trả lời 1 câu duy nhất tại từng thời điểm: "ngay bây giờ, gate đang đóng hay đang mở?". Đây là lớp kiểm tra thứ 2 trong thiết kế, tách biệt hoàn toàn khỏi lớp thời gian (Mục 14 giả định gate mất ~10 giây để đóng — con số đó chỉ dùng để *lên lịch trước*). Quy tắc cứng: dù đã đợi đủ 10 giây theo lịch, hệ thống **không bao giờ** tự suy ra "chắc đóng rồi" — nó luôn chờ sensor #6 xác nhận thật; nếu tới hạn mà sensor #6 vẫn chưa báo `GATE_CLOSED_CONFIRMED`, đó là lỗi (Invariant #6, Mục 22), không phải "đóng trễ 1 chút cũng được". Phím bấm mô phỏng lỗi. |

Chỉ 6 loại sensor này được đưa vào thiết kế Core. Đếm xe, ANPR, sensor thời tiết, sensor riêng theo từng làn (xem Mục 5b) v.v. đã được cân nhắc và loại bỏ khỏi Core — xem Mục 23.

**Tóm lại 2 lớp trong toàn bộ chuỗi an toàn đường sắt (Mục 14):**
1. **Lớp thời gian (kế hoạch):** các con số giả định như "gate mất ~10 giây để đóng", "tổng thời gian dẫn trước ~45 giây" — chỉ dùng để *tính lịch trước*, không bao giờ dùng để ra quyết định an toàn.
2. **Lớp kiểm tra (thực tế):** sensor #6 xác nhận trạng thái gate thật tại đúng thời điểm cần quyết định — đây mới là thứ quyết định `train signal` có được `PROCEED` hay không.
Tách 2 lớp này ra là chủ đích, để không bao giờ có chuyện "hết giờ theo lịch → coi như an toàn" mà không kiểm tra lại thực tế.

---

## 10. Actuator và tín hiệu

| # | Actuator | Chủ sở hữu | Trạng thái | Trạng thái an toàn |
|---|---|---|---|---|
| 1 | Đầu đèn tín hiệu xe (mỗi approach) | `Lx` | `RED / YELLOW / GREEN / FLASHING_RED` | `FLASHING_RED` (nhấp nháy toàn bộ các hướng, theo quy ước mất điện của đèn giao thông thật — buộc mọi approach phải nhường đường) |
| 2 | Đèn tín hiệu người đi bộ (mỗi bên lối qua) | `Lx` | `WALK / FLASHING_DONT_WALK / DONT_WALK` | `DONT_WALK` |
| 3 | Boom gate (mỗi hướng) | `RLx` | `UP / MOVING / DOWN / FAULT` | `DOWN` (mọi sự mơ hồ luôn được giải quyết theo hướng chặn đường, không bao giờ theo hướng cho tàu qua khi chưa xác nhận) |
| 4 | Đèn nhấp nháy đường sắt (mỗi hướng approach của đường) | `RLx` | `ON / OFF` | `ON` |
| 5 | Train signal (mỗi hướng ray) | `RLx` | `STOP / PROCEED` | `STOP` |

Không có actuator mũi tên rẽ riêng trong thiết kế Core (Mục 5b, Mục 23). Phương án 3 đèn mũi tên/approach (làn trái/thẳng/phải riêng biệt) được tài liệu hoá như 1 lựa chọn thiết kế đã cân nhắc — xem Mục 5b — nhưng chỉ là hướng nâng cấp tương lai, không phải actuator Core.

---

[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · [← Tầm nhìn và phạm vi](01-tam-nhin-va-pham-vi.md) · Tiếp theo: [Vận hành và hành vi →](03-van-hanh-va-hanh-vi.md)
