[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · Chương 4/6 · [← Vận hành và hành vi](03-van-hanh-va-hanh-vi.md) · Tiếp theo: [Tính năng và demo →](05-tinh-nang-va-demo.md)

---

## 17. Hành vi lỗi và an toàn

| Lỗi | Phản ứng |
|---|---|
| Gate không xác nhận `CLOSED` trước deadline trong khi có tàu đang tới | `FAULT_SAFE` (crossing): `RLx` lập tức ép/giữ train signal ở `STOP`, **đồng thời (không phải bước sau, không phải điều kiện trước)** báo lỗi cho `C1`; hướng đi về crossing bị giữ đỏ cho tới khi được xoá lỗi thủ công. `RLx` không bao giờ đợi `C1` xác nhận rồi mới ép `STOP` (Mục 18). |
| Sensor tiếp cận tàu bị kẹt **active** (cứ báo `TRAIN_APPROACHING` mãi không hết) | **Phát hiện được.** Nếu tín hiệu vẫn còn active quá lâu so với ngân sách thời gian dẫn trước + chiếm dụng đã suy ra (Mục 14) mà không có tàu nào được xác nhận đã qua, `RLx` coi đây là bất thường, báo lỗi, và fail theo hướng an toàn — giữ crossing ở `WARNING`/gate xuống cho tới khi operator xoá lỗi. Đúng nguyên tắc thực tế: thiết bị chắn đường sắt luôn fail theo hướng chặn xe. |
| Sensor tiếp cận tàu **im lặng hoàn toàn** (ngừng báo tín hiệu, "chết") | **Giới hạn được chấp nhận — thiết kế Core KHÔNG phát hiện được.** 1 sensor đã ngừng hẳn báo tín hiệu, dưới góc nhìn của `RLx`, giống hệt "hiện không có tàu nào tới" — thiết kế sensor Core không có tín hiệu heartbeat/self-test độc lập để phân biệt 2 tình huống này. Nghĩa là 1 sensor chết hẳn sẽ âm thầm làm crossing đó mất khả năng cảnh báo tàu chủ động — đây là hướng **fail-open**, không phải fail-safe, và thiết kế không tuyên bố phát hiện được điều này. Ghi nhận thẳng thắn thay vì ngầm bỏ qua; hướng khắc phục đã xác định (thêm 1 tín hiệu self-test/heartbeat định kỳ, độc lập với đường tín hiệu phát hiện tàu) nằm ngoài phạm vi Core (Mục 26). |
| Gate kẹt mở khi có tàu đang tới | Train signal bị ép `STOP`; `C1` báo động **critical**; các giao lộ liền kề chỉ nhận lỗi để ghi log tình huống — đặc tả nói rõ **không có hành động phần mềm nào ở đây có thể dừng tàu về mặt vật lý**; đây là 1 giới hạn thật sự, không phải thứ hệ thống tuyên bố khắc phục được |
| Gate kẹt đóng, không có tàu | Báo về `C1` như 1 lỗi gây phiền (nuisance fault); train signal giữ nguyên mặc định an toàn; xe cộ đơn giản là chờ (an toàn, chỉ kém hiệu quả); cần operator xoá lỗi thủ công sau khi sửa vật lý — `C1` không thể ép gate vật lý di chuyển (Safety Invariant: không remote override thiết bị đường sắt) |
| 1 sensor xe/người đi bộ lỗi | Được công bố sau 1 khoảng im lặng dài bất thường so với chu kỳ dự kiến; approach đó fail-safe về "giả định có nhu cầu" để không bao giờ bị bỏ đói; không ảnh hưởng phần còn lại của giao lộ |
| Phần mềm local controller lỗi | Fail về trạng thái an toàn của actuator (Mục 10) — hiện thực trong PoC qua **[HD]** 1 QNX watchdog/supervisor process nhẹ trên mỗi node (Mục 23) phát hiện controller process đã chết và ép output về an toàn |
| Mất liên lạc, `Lx ↔ C1` | `DEGRADED_LOCAL` (Mục 11) |
| Mất liên lạc, `Lx ↔ Lx'` (phối hợp giữa các peer) | Mọi tính năng phối hợp (offset green-wave) suy giảm về timing độc lập (standalone); hành vi an toàn cốt lõi của cả 2 controller hoàn toàn không bị ảnh hưởng — không controller nào bị block chờ controller khác vì lý do an toàn |

---

## 18. Hành vi Central Controller

`C1` giám sát trạng thái, timing profile, lỗi và trạng thái đường sắt của cả **9 local controller** (`L1-L6` + `RL1-RL3` — `C1` không tự giám sát chính nó, nên là 9, không phải tổng 10 logical controller của toàn mạng lưới), và phát ra 3 nhóm lệnh: `SET_MODE`, `SET_TIMING_PROFILE`, `REQUEST_OVERRIDE` [REQ/CLARIF — "command abstraction," không bao giờ là 1 màu đèn thô]. Mỗi lệnh nhận về 1 ACK/NACK rõ ràng. NACK nghĩa là local controller đánh giá lệnh đó không an toàn (ví dụ 1 yêu cầu override trong lúc railway pre-emption đang diễn ra tại giao lộ đó) và điều này được hiển thị cho operator như 1 lệnh bị từ chối/báo lỗi, không bao giờ bị âm thầm bỏ qua.

`C1` quyết định mode theo giờ trong ngày cho toàn mạng lưới như nguồn thông tin mặc định (để đồng bộ khi demo), nhưng mọi local controller vẫn giữ và dùng đồng hồ dự phòng của riêng nó ngay khi `C1` không thể liên lạc được (Mục 11) — đây chính là điều khiến "Central quyết định mode" và "local phải an toàn khi không có Central" cùng đúng một lúc.

`REQUEST_OVERRIDE(CLEAR_ROUTE, mục tiêu, thời lượng)` **[TEAM]**: gửi tới local controller `Lx` của giao lộ mục tiêu; ép 1 hướng đi chỉ định thành xanh trong thời lượng giới hạn (mặc định tối đa 5 phút **[ASSUM]**, tự hết hạn về mode trước đó nếu không được gia hạn — tránh 1 override bị quên làm hỏng vận hành bình thường vĩnh viễn). Nó bị từ chối thẳng nếu giao lộ mục tiêu đang có railway pre-emption đang diễn ra/sắp diễn ra trên hướng đó, và không bao giờ cắt ngang 1 pedestrian clearance đang diễn ra (Mục 22).

**Báo lỗi đường sắt không bao giờ đi vòng qua `C1` [TEAM].** Khi `RLx` phát hiện lỗi thiết bị đường sắt (Mục 17), việc ép train signal về `STOP` và việc báo lỗi cho `C1` là 2 hành động **độc lập, song song** — `RLx` không bao giờ đợi `C1` xác nhận rồi mới ép `STOP`. Điều này khớp với mô tả của giảng viên (tàu nhận tín hiệu dừng ngay khi có sự cố; lỗi *sau đó* mới được báo về control room) và giữ đúng nguyên tắc tự chủ local (Mục 19) cho quyết định an toàn quan trọng nhất hệ thống. Vai trò duy nhất của `C1` sau khi nhận lỗi là hiển thị nó, và khi sửa vật lý xong, gửi 1 yêu cầu xoá lỗi đã được xác thực tới `RLx` (Mục 8.1) — `RLx` vẫn có quyền từ chối nếu tình trạng lỗi thật ra chưa hết.

---

## 19. Quyền tự chủ local

Mọi quyết định liên quan an toàn mà `Lx` hoặc `RLx` đưa ra — chia pha không xung đột, timing clearance, phản ứng railway pre-emption, gate-confirmed-before-proceed — đều được tính toán **hoàn toàn ở local**, không phụ thuộc runtime vào việc `C1` có liên lạc được hay không. Khi mất link với Central:

- **Không** làm dừng vận hành sensor-driven (`OFF_PEAK_SENSOR` vẫn tiếp tục dùng input sensor local thuần tuý).
- **Không** làm dừng bảo vệ đường sắt (logic của `RLx` không phụ thuộc Central ở bất kỳ điểm nào trong state machine).
- Khiến controller vào `DEGRADED_LOCAL`: **giữ nguyên tham số timing hợp lệ gần nhất** (thời lượng xanh/vàng/all-red, trần gia hạn, offset phối hợp) thay vì quay về 1 mặc định cứng tuỳ tiện — nhưng **mode (Peak/Off-Peak) không bị đóng băng theo "mode gần nhất"**: từ lúc mất kết nối, đồng hồ local của controller trở thành thẩm quyền duy nhất để chuyển `PEAK_FIXED ↔ OFF_PEAK_SENSOR`, y hệt như khi còn `C1`, chỉ là dùng profile timing đã xác thực gần nhất cho mode đang được chọn.
- Khi kết nối lại, **local controller đẩy toàn bộ trạng thái thực tế hiện tại lên `C1`** (không phải chiều ngược lại) — điều này tránh khoảng thời gian mà `C1` có thể áp 1 lệnh cũ lên trạng thái đã thay đổi.

Mất link peer-to-peer giữa 2 local controller (chỉ dùng cho phối hợp tuỳ chọn, Mục 20) không bao giờ ảnh hưởng hành vi an toàn cốt lõi của bất kỳ controller nào — phối hợp suy giảm nhẹ nhàng về standalone; nó không bao giờ là điều kiện để giữ an toàn.

---

## 20. Chiến lược timing và phối hợp

**Phương pháp, không phải số bịa đặt [ASSUM]:** chu kỳ, tỷ lệ chia pha, và offset được suy ra từ giả định khoảng cách/tốc độ ở Mục 5, theo cách tiếp cận chuẩn mà chính đề bài gợi ý (đo khoảng cách giữa các giao lộ, giả định tốc độ di chuyển, suy ra thời gian 1 tốp xe tới đèn tiếp theo, dịch thời điểm bắt đầu xanh của đèn đó theo thời gian di chuyển đó — tức là 1 green wave). Giá trị cuối cùng là **tham số có thể điều chỉnh**, không tuyên bố là số liệu thực tế đã được kiểm chứng.

**Ví dụ tính cụ thể [ASSUM]** (đề bài nhắc 2 lần — cả brief gốc lẫn lecture clarification — rằng khoảng cách phải được đo/ước lượng để suy ra timing, nên mình tính hẳn ra số thay vì chỉ nêu công thức): giả định khoảng cách `I1–I3 = 350m`, `I3–I5 = 400m` trên `R1`; `I2–I4 = 320m`, `I4–I6 = 380m` trên `R2`; tốc độ arterial 60 km/h = 16.67 m/s.
- Offset `L1→L3` = 350 / 16.67 ≈ **21 giây**
- Offset `L3→L5` = 400 / 16.67 ≈ **24 giây** (cách `L1` tổng cộng 45 giây)
- Offset `L2→L4` = 320 / 16.67 ≈ **19 giây**
- Offset `L4→L6` = 380 / 16.67 ≈ **23 giây**

**Phạm vi phối hợp [TEAM]:** offset chỉ được tính cho các chuỗi arterial (`I1–I3–I5` trên `R1`, `I2–I4–I6` trên `R2`) — việc phối hợp các connector (`R3/R4/R5`) qua hành lang đường sắt đã được cân nhắc và loại bỏ, vì những con đường này ngắn, tính liên tục thấp, và yếu tố chi phối timing của chúng là railway pre-emption, không phải sự tiến triển của tốp xe (Mục 23).

**Lưu ý quan trọng — đây là 2 khái niệm "phối hợp" KHÁC NHAU, không liên quan:** `TC` ở trên là phối hợp giữa các **giao lộ đường bộ** trên cùng 1 trục arterial (`Lx`). Còn `RL1`, `RL2`, `RL3` (3 chỗ giao đường sắt) thì **hoàn toàn độc lập với nhau** — không chia sẻ trạng thái, không có mô hình "1 tàu chạy chéo kích hoạt tuần tự cả 3 crossing" nào cả. Mỗi `RLx` chỉ biết và phản ứng với tàu tại crossing của chính nó (xem Mục 8.3).

**Ai tính toán [TEAM]:** `C1` tính toán và phân phối profile offset (nó có cái nhìn toàn mạng lưới cần thiết cho phép tính), nhưng mỗi `Lx` tự áp offset vào sequencer local của mình — vậy nên phối hợp chỉ là 1 *tham số timing* được gửi qua cùng nhóm lệnh `SET_TIMING_PROFILE` như mọi thứ khác, không phải 1 loại lệnh mới hay 1 quyền mới cho `C1`.

**Lỗi phối hợp [TEAM]:** bất kỳ `Lx` nào mất input offset (Central không liên lạc được, hoặc báo cáo của peer bị cũ) sẽ quay về timing standalone fixed/sensor — không bao giờ block chờ 1 peer.

**Gián đoạn green-wave do đường sắt [TEAM]:** sau 1 sự kiện pre-emption, đồng hồ pha của `Lx` bị ảnh hưởng tiếp tục chạy, và `SET_TIMING_PROFILE` tiếp theo nhận được (hoặc lần đẩy trạng thái khi kết nối lại) sẽ tái lập offset — thiết kế **không** cố xây 1 giai đoạn tái đồng bộ đặc biệt, vì làm vậy chỉ thêm 1 state machine mà không có giá trị an toàn/demo độc lập nào ngoài "phối hợp cuối cùng cũng phục hồi", điều mà đường đi bình thường đã đảm bảo sẵn (Mục 23 — Rejected: thuật toán resync dần dần).

---

## 21. Yêu cầu real-time

| Sự kiện | Trigger | Bản chất | Deadline (mục tiêu) | Hành vi khi trễ |
|---|---|---|---|---|
| Bắt đầu chuỗi an toàn đường sắt | Sensor tiếp cận kích hoạt | Hard, không định kỳ | Phản ứng local trong 200 ms; thông báo giao lộ liền kề trong 500 ms toàn trình (biên rất rộng so với ngân sách thời gian dẫn trước vật lý ~45 giây, Mục 14) | N/A — đây là deadline duy nhất mà toàn bộ thiết kế được xây để không bao giờ trễ; phản ứng local là 1 đường sensor-tới-actuator đơn giản, không phụ thuộc từ xa |
| Xác nhận gate đóng trước khi train-proceed | Sensor vị trí gate | Hard | Phải xác nhận trước deadline suy ra (Mục 14) | Train signal mặc định về `STOP`, báo lỗi (fail-to-safe, không bao giờ fail-to-proceed) |
| Tick của phase sequencer | Timer nội bộ | Soft, định kỳ | Tick 100 ms, dung sai jitter ±50 ms | Chỉ ảnh hưởng độ mượt về mặt hình thức; không ảnh hưởng an toàn |
| Nút bấm người đi bộ → đăng ký latch | Bấm nút | Soft | < 500 ms (ngưỡng con người cảm nhận được) | Chỉ chậm xác nhận, yêu cầu không bao giờ bị mất |
| Đèn đổi trạng thái → báo cáo `C1` | Bất kỳ thay đổi trạng thái đèn/người đi bộ/mode | Firm — được đề bài yêu cầu rõ ràng [REQ] | < 200 ms | Ghi log như 1 dấu hiệu suy giảm liên lạc; không ảnh hưởng an toàn local |
| Heartbeat | Liveness định kỳ | Firm, định kỳ | Chu kỳ 1 giây; mất 3 lần liên tiếp (~3 giây) ⇒ công bố mất link | Kích hoạt `DEGRADED_LOCAL` |
| Lệnh Central → ACK/NACK | `SET_MODE`/`SET_TIMING_PROFILE`/override | Soft/firm | < 1 giây | 2 lần retry, sau đó `C1` đánh dấu controller không liên lạc được/báo động liên lạc |
| Phát hiện lỗi → báo động | Bất kỳ lỗi sensor/gate/phần cứng | Hard (bản thân nó là 1 báo cáo an toàn) | Nhanh hơn đường status bình thường — gửi ngay, không gộp batch | N/A — báo lỗi không bao giờ bị giới hạn tốc độ |
| Override được áp dụng | `REQUEST_OVERRIDE` được chấp nhận | Soft, cố ý bị giới hạn bởi an toàn, không tức thời | Áp dụng tại ranh giới pha an toàn tiếp theo (không có deadline cố định, có chủ đích) | N/A — có chủ đích: 1 override *không bao giờ* được áp dụng tức thời nếu điều đó bỏ qua 1 khoảng clearance |

**Scale thời gian khi demo [TEAM]:** 1 hệ số tăng tốc toàn cục, có thể cấu hình, được áp dụng đồng đều cho mọi timer trong hệ thống (thời lượng pha, khoảng người đi bộ, thời gian dẫn trước đường sắt, ngưỡng heartbeat/timeout) để các biên an toàn tương đối được giữ nguyên ở tốc độ demo — việc chỉ scale 1 số timer (ví dụ giao thông nhưng không scale đường sắt) đã bị cân nhắc và loại bỏ vì sẽ làm sai lệch chính các biên an toàn mà buổi demo cần chứng minh.

---

## 22. Safety Invariant (bất biến an toàn)

Đây là các quy tắc mà state chart, task architecture, và test scenario đều phải truy vết về được:

1. Không có 2 hướng xe xung đột nào tại cùng 1 giao lộ được nhận `GREEN` đồng thời (xác định bởi ma trận xung đột tĩnh của giao lộ đó).
2. Không có `WALK` cho người đi bộ nào được kích hoạt đồng thời với 1 hướng xe `GREEN`/`YELLOW` xung đột với lối qua đó.
3. Mọi lần chuyển giữa các hướng xung đột đều phải qua `YELLOW` rồi `ALL_RED` với khoảng thời gian tối thiểu đã cấu hình trước khi hướng xung đột tiếp theo được `GREEN`.
4. Khi 1 pha `WALK` đã bắt đầu, nó phải hoàn tất toàn bộ clearance trước khi lối qua đó phục vụ lại 1 `GREEN` xe xung đột.
5. Train signal không bao giờ hiển thị `PROCEED` trừ khi boom gate của crossing đó đã được sensor xác nhận `CLOSED`.
6. Nếu không thể xác nhận gate đã đóng trước deadline yêu cầu, train signal mặc định về `STOP` và báo lỗi — mọi sự mơ hồ luôn được giải quyết về trạng thái an toàn hơn, không bao giờ "giả định là ổn".
7. Chỉ `RLx` sở hữu mới được actuate boom gate, đèn nhấp nháy, hoặc train signal của nó — không controller nào khác, kể cả `C1`, được phát lệnh actuate thiết bị đường sắt.
8. Gate không được mở lại khi cửa sổ chiếm dụng crossing tính toán được của bất kỳ đoàn tàu nào (Mục 14, bước 6) chưa hết hạn, ở bất kỳ hướng nào — kể cả 1 đoàn tàu thứ hai được phát hiện trong khi đoàn đầu vẫn đang chiếm dụng crossing.
9. 1 hướng đi dẫn thẳng vào crossing không được hiển thị `GREEN` khi crossing đó đang `CLOSING`, `CLOSED`, `TRAIN_PRESENT`, hoặc `FAULT`.
10. Mọi local controller phải chạy được đầy đủ logic liên quan an toàn (phân pha, clearance, phản ứng đường sắt) khi `C1` không liên lạc được.
11. 1 lệnh từ `C1` vi phạm bất kỳ invariant nào ở trên phải bị từ chối và báo cáo bởi controller nhận lệnh, không bao giờ được âm thầm áp dụng.
12. 1 override không bao giờ được cắt ngang 1 pedestrian clearance đang diễn ra và không bao giờ được ra lệnh 1 hướng đi về crossing khi crossing đó chưa `OPEN`.
13. Mất 1 sensor hoặc 1 đường liên lạc đơn lẻ có thể làm suy giảm chức năng (ví dụ về fixed timing, hoặc về phối hợp standalone) nhưng không bao giờ tự nó tạo ra 1 trạng thái output không an toàn.

### Thứ tự ưu tiên khi nhiều điều kiện xảy ra cùng lúc — **[TEAM, đã chốt ngày 2026-08-23]**

**Ghi chú minh bạch:** bản nháp trước lấy gần như nguyên vẹn 1 gợi ý xếp hạng từ `idea.md`. Nhưng đúng như team đã chỉ ra, `idea.md` chỉ là danh sách câu hỏi do 1 AI khác tạo ra để gợi mở, chưa từng được team xác nhận hay kiểm chứng dựa trên hành vi thật của hệ thống — dùng nó làm căn cứ cho 1 phần an toàn cốt lõi là không đủ chắc chắn. Phần dưới đây được **suy luận lại từ đầu**, bằng cách xét từng cặp điều kiện xem chúng có thật sự tranh chấp cùng 1 output hay không, dựa trên hành vi cụ thể đã mô tả ở các Mục 11–21.

**Bước 1 — Đây là các MODE loại trừ lẫn nhau thật sự (1 controller chỉ ở đúng 1 trạng thái này tại 1 thời điểm):**

```text
1. FAULT_SAFE                    — không thực sự "xếp hạng" cùng các mode khác; đây là quyền
                                    phủ quyết tuyệt đối cục bộ, áp dụng khi controller không
                                    còn tin được vào output của chính nó
2. RAILWAY_PREEMPTION            — chồng lên mode đang chạy, nhưng CHỈ ảnh hưởng approach
                                    hướng về crossing (Mục 15), không ảnh hưởng cả giao lộ
3. CENTRAL_OVERRIDE              — có thể chồng lên mode nền, nhưng bị TỪ CHỐI nếu xung đột
                                    với RAILWAY_PREEMPTION đang diễn ra (Mục 18) — vì vậy luôn
                                    đứng sau mục 2
4. PEAK_FIXED / OFF_PEAK_SENSOR  — mode nền, chạy khi không có 3 mục trên
```

(`DEGRADED_LOCAL` nằm ngoài trục này — nó mô tả "còn liên lạc được với Central hay không", không phải 1 hazard cần xếp hạng.)

**Bước 2 — Đây là RÀNG BUỘC áp dụng cho MỌI mode ở Bước 1, không phải 1 "mode" cạnh tranh riêng — xếp chúng thành các bậc ưu tiên riêng (như bản nháp cũ) là không chính xác:**

- **Pedestrian clearance đang diễn ra phải luôn được chạy hết** (Invariant #4, #12), bất kể đang ở mode nào. Sau khi xét kỹ theo đúng cách vận hành đã mô tả ở Mục 15: `RAILWAY_PREEMPTION` chỉ ép **1 hướng xe cụ thể** (hướng đi về crossing) chuyển đỏ — nó không bao giờ cần bật xanh cho hướng đang xung đột với 1 pedestrian WALK đang chạy. Vì vậy, trên thực tế, pedestrian clearance **không bao giờ thật sự tranh chấp** với `RAILWAY_PREEMPTION` — đây là 1 ràng buộc luôn đúng ở mọi lúc, không phải 1 bậc cần so sánh cao/thấp hơn mục 2.
- **Ngừng cấp nhu cầu ùn tắc + pha drain (Mục 16)** là hành vi *bên trong* của `PEAK_FIXED`/`OFF_PEAK_SENSOR`, không phải 1 mode riêng đứng ngang hàng để cạnh tranh với `CENTRAL_OVERRIDE`.
- **Yêu cầu người đi bộ và nhu cầu sensor xe được xử lý NGANG NHAU** khi lên lịch pha — đây là điều đã nói rõ ở Mục 13 ("1 yêu cầu đã latch được xử lý y như nhu cầu của xe khi lên lịch"). Bản nháp cũ xếp 2 thứ này thành 2 bậc riêng (mục 6 và 7, người đi bộ cao hơn xe) — **điều này mâu thuẫn trực tiếp với Mục 13** và đã được sửa ở đây.

**Kết luận chính thức (đã chốt):**

```text
Trục ưu tiên MODE (loại trừ lẫn nhau):
  1. FAULT_SAFE                      - quyền phủ quyết tuyệt đối cục bộ
  2. RAILWAY_PREEMPTION              - chỉ ảnh hưởng approach hướng về crossing
  3. CENTRAL_OVERRIDE                - bị từ chối nếu xung đột với mục 2
  4. PEAK_FIXED / OFF_PEAK_SENSOR    - mode nền

Ràng buộc áp dụng cho MỌI mode ở trên (không phải bậc ưu tiên riêng):
  - Pedestrian clearance đang diễn ra không bao giờ bị cắt ngang
  - Ngừng cấp nhu cầu ùn tắc + pha drain là logic nội bộ của mode nền
  - Yêu cầu người đi bộ và nhu cầu sensor xe được xử lý ngang nhau
```

Team đã xác nhận: người đi bộ chỉ băng qua đường ở giao lộ, không bao giờ băng qua đường ray, nên pedestrian WALK và `RAILWAY_PREEMPTION` không có tình huống nào thật sự xung đột trực tiếp. Đồng thời xác nhận mâu thuẫn với Mục 13 (bản cũ xếp "yêu cầu người đi bộ" cao hơn "nhu cầu sensor xe", trong khi Mục 13 nói rõ 2 cái xử lý ngang nhau) là có thật và đã được sửa đúng ở bản chốt này.

---

[← Mục lục](../PROJECT_SPECIFICATION.vi.md) · [← Vận hành và hành vi](03-van-hanh-va-hanh-vi.md) · Tiếp theo: [Tính năng và demo →](05-tinh-nang-va-demo.md)
