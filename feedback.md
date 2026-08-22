Ừ, t đã đọc `PROJECT_SPECIFICATION.md` như **source of truth**, dùng `SYSTEM_DIAGRAMS.md` để check cách visualize/naming, rồi đối chiếu toàn bộ `system_assumptions_tables.md`. Overall thì **project spec bây giờ coherent hơn rất nhiều**, và phần assumptions hiện tại **đã đủ tốt để đưa vào report sau khi sửa vài điểm**. T **chưa recommend copy nguyên xi ngay** vì có khoảng **5 chỗ cần chỉnh**, trong đó 2 chỗ là logic inconsistency thật sự.

T sẽ giữ context này làm baseline cho các phần tiếp theo: `I1–I6` = physical intersections, `L1–L6` = local intersection controllers, `RC1–RC3` = physical railway crossings, `RL1–RL3` = railway controllers, `C1` = Central; Core PoC = `L1/L2/RL1 + C1`, committed HD extension = `L3/L4/RL2`, full third slice optional.  Naming distinction cũng consistent với system diagrams. 

## 1. Overall project specification: ✅ khá ổn

Core story bây giờ rất rõ:

**local authority + railway fail-safe interlock + distributed coordination + graceful degradation**, thay vì cố biến nó thành traffic simulator. Project spec explicitly loại stochastic vehicle simulation và dùng sensor events làm input, cái này consistent từ vision tới demo. 

HD scope cũng hợp lý chứ không phải add feature random:

- second network slice để demonstrate actual green-wave;
- advance queue sensor + congestion suppression;
- unsafe-command NACK;
- watchdog;
- bounded override.

Các feature này đều có demo scenario cụ thể, nên đúng tinh thần đề là claim gì thì demo được cái đó.  

**Không thấy architectural contradiction lớn kiểu phải redesign project.**

Nhưng có vài issue cụ thể.

---

# 2. ISSUE #1 — Central đang “monitor all 10 controllers” ❌

Trong architecture của m:

```text
9 local controllers:
L1-L6 = 6
RL1-RL3 = 3

+ C1 = Central
----------------
10 logical controllers total
```

Cái này đúng. 

Nhưng Section 18 lại ghi:

> `C1 monitors all 10 controllers' state...`



Không đúng về wording vì C1 đâu monitor chính nó theo architecture đang mô tả.

Nên phải thành:

> **C1 monitors the state of all nine local controllers...**

Hoặc:

> **C1 maintains a network-wide view of all nine local controllers...**

Đây là typo/conceptual slip nhỏ thôi nhưng nên sửa trong `PROJECT_SPECIFICATION.md`.

---

# 3. ISSUE #2 — PA-08 đang nói hai thứ hơi conflict nhau ⚠️

Assumption hiện tại nói:

> khi mất C1, local continues on its **last known valid mode/timing profile**  
> **and independently self-selects Peak/Off-Peak using its own local clock.**



Hai câu này cần clarify.

Ví dụ:

```text
Trước disconnect:
C1 says PEAK_FIXED
profile = X

30 phút sau local clock says OFF-PEAK
```

Nó:

**A. giữ PEAK_FIXED vì “last known mode”?**

hay

**B. đổi sang OFF_PEAK_SENSOR bằng local clock?**

Project spec dường như muốn **B**, vì Section 19 cũng nói local tự select Peak/Off-Peak bằng local clock. 

Vậy assumption nên precise thành:

> local retains the **last known valid timing parameters**, but mode selection thereafter follows its own local time-of-day schedule.

Tức:

```text
disconnect
   ↓
retain validated config/timing parameters
   +
local clock becomes mode authority
   ↓
PEAK ↔ OFF-PEAK vẫn đổi bình thường
```

Không nên ghi “last known mode” nữa.

---

# 4. ISSUE #3 — Congestion drain logic chưa đủ deterministic ⚠️

CC-03 hiện ghi:

> drain phase duration is **sized using the advance/queue sensor's state**



Nhưng CC-01 định nghĩa sensor đó chỉ là binary:

```text
QUEUE_WARNING = yes/no
```

Nó **không biết**:

- queue có 5 xe;
- 10 xe;
- 20 xe;
- dài 70m hay 100m.

Cho nên "sized using sensor state" hơi overstated.

M chỉ biết:

```text
queue reached sensor? YES / NO
```

Nên cần một rule rõ hơn. Ví dụ conceptual:

```text
crossing opens
    ↓
QUEUE_WARNING still active?
    ↓ yes
connector receives extended drain green
    ↓
check again at bounded interval
    ↓
stop when:
QUEUE_WARNING clears
OR
max drain duration reached
```

Như vậy sensor binary là đủ.

Nếu không thì lecturer có thể hỏi:

> “How exactly do you calculate drain duration from a Boolean sensor?”

và hiện tại spec chưa trả lời được.

**T recommend sửa CC-03**, không cần redesign feature.

---

# 5. ISSUE #4 — “Central cannot control railway” đang hơi quá absolute

Đây là chỗ đáng chú ý nhất khi compare với lecturer clarification.

Ừ, **m nhớ gần đúng nhưng có 2 ý khác nhau đang bị trộn lại**.

Trong đề chính thức có một câu rất rõ:

> “Operators in the central control room will occasionally send commands to the intersections, boom gate control and train approach signals system.” 

Nên **đề thật sự có mention Central có thể send commands tới**:
- intersections,
- boom gate control,
- train approach signals system.

Nhưng đồng thời đề cũng nói rất rõ boundary:

> Central **does not directly control the individual lights at an intersection**; việc đó luôn do local controller làm. 

Và lecturer clarification m ghi lại cũng nói tương tự cho railway:
- intersection local controller chỉ **sense railway state**
- chỉ **railway local controller** mới control boom gates, flashing lights, train signal.

Vậy cách hiểu an toàn nhất là:

```text
Central
   ↓ high-level command/request
Local controller / railway controller
   ↓ validates + executes
Physical light / boom gate / train signal
```

Chứ **không phải**:

```text
Central
   ↓
directly actuates boom gate / train light
```

Còn flow m nhớ về boom-gate fault thì lecturer clarification support kiểu này:

```text
Boom gate / gate sensor detects issue
          ↓
Railway local controller (RLx)
          ↓
sets train signal = RED / STOP
          ↓
reports fault to Central control room
```

Ở đây điểm quan trọng là: **Central không cần gửi lệnh STOP xuống train sau khi nhận fault**. Theo lời thầy, railway local controller có quyền control train signal và phải tự fail-safe locally. Error thì report lên Central. Lecturer clarification của m là “if there any issue here, then the light for the train will be red” và error “should be reported to the control room”, chứ không nói Central phải round-trip rồi mới command train dừng.

Nên flow tốt hơn là:

```text
Gate fault
   ↓
RLx detects fault
   ├──> Train signal = RED immediately
   └──> Report fault to C1
```

chứ không nên là:

```text
Gate fault
→ RLx
→ C1
→ C1 tells train to stop
```

vì cái flow sau tạo dependency vào Central/comms cho một safety-critical action, trái với local-autonomy principle.

Tóm lại: **có câu Central sends commands to boom gate control/train approach signal system thật**, nhưng nên interpret là **high-level command tới railway controller**, không phải direct actuation. Còn **boom-gate fault → train red** thì RLx nên xử lý locally rồi report lên Central, không đợi Central phản hồi.

---

# 6. ISSUE #5 — regular vehicle sensor vs queue sensor wording

PA-05 viết:

> Vehicle-presence sensors are monitored continuously in PEAK_FIXED but only influence timing in OFF_PEAK_SENSOR; in PEAK_FIXED their role is limited to **congestion/fault detection**.



Nhưng congestion detection chính thức của design lại dùng **advance/queue sensor** riêng. 

Nên câu này làm hơi confuse.

T sẽ đổi:

> ordinary stop-line vehicle-presence sensors are monitored continuously for **status/fault purposes**, but affect phase scheduling only in `OFF_PEAK_SENSOR`.

Còn:

> congestion warning is provided separately by the advance/queue sensor defined in CC-01.

Clear hơn nhiều.

---

# 7. Còn `system_assumptions_tables.md` itself thì sao?

## Về coverage: ✅ đủ và đúng requirement

Teacher yêu cầu assumptions về:

- timing;
- traffic coordination;
- congestion control;
- differences in traffic demand/priority R1–R5.

File m có hẳn:

```text
1.2.1 Network / Road Usage
1.2.2 Demand / Priority
1.2.3 Traffic-Light Timing
1.2.4 Traffic Coordination
1.2.5 Congestion Control
1.2.6 Railway Crossing
1.2.7 Pedestrian / Sensing / Abnormal
```

Nên coverage **dư sức đáp ứng wording requirement**. 

Đặc biệt DP section bây giờ tốt hơn draft cũ nhiều vì m **không fake vehicles/hour** nữa; thay bằng qualitative arterial vs connector priority và binary sensor demand. 

---

# 8. Có một thứ t sẽ REMOVE trước khi paste report

Đầu file có:

> **Revision note:** this table replaces an earlier draft...

và liệt kê:

- old 60s cycle;
- old vehicle/hour;
- old distances;
- excluded left turns;
- diagonal propagation model...



**Cái này không nên nằm trong submitted report.**

Lecturer không cần biết AI/team từng generate một draft sai rồi sửa.

Delete toàn bộ **Revision note**.

Formatting convention có thể giữ nếu thực sự dùng dấu `*`, nhưng revision history thì bỏ.

---

# 9. Appendix-ish wording trong assumptions hơi internal quá

Một vài chỗ viết kiểu:

> “consistent with project's duplicate by configuration principle”

> “Safety Invariant #7”

> “see Section 5b”

> “Core decision logic”

Những cái này **okay trong project spec**, nhưng khi đưa vào Initial Design Report phải chắc chắn report thực sự có corresponding:

```text
Safety Invariant #7
Section 5b
Core design definition
HD-target feature definition
```

Nếu report không có exact section numbering đó → dangling reference.

Ví dụ RC-01 hiện refer:

> `(Safety Invariant #7)` 

Trong project spec thì invariant #7 tồn tại. 

Nhưng **Initial Design Report outline hiện tại của m chưa có “Section 22 Safety Invariants”**.

Cho nên khi copy sang report:

either:

> bỏ `(Safety Invariant #7)`

hoặc sau này tạo subsection System Safety Rules và cross-reference đúng report numbering.

---

# 10. Một claim external t chưa cho m pass 100%

TC-01 / References dùng:

> QCVN 41:2019/BGTVT

để support 50/60 km/h context. 

Ở đây t **chưa verify external source**, nên t không muốn nói với m “citation này chắc chắn đúng”.

Với report thật, citation `[3]` phải được verify:

- đúng document không;
- version còn current 2026 không;
- document đó thực sự support speed values m đang claim không.

**Content design vẫn okay**, vì 50/60 km/h được labelled assumption. Chỉ là citation academic cần check trước submit.

---

# 11. One project-spec concern nữa: train sensor failure

Spec nói:

> Train sensor failure (no signal / stuck) → assume train may be present, gate stays down.



“stuck active” detect được.

Nhưng **“no signal” / failed-silent sensor** khá khó detect từ một sensor duy nhất:

```text
sensor says nothing
```

có thể là:

- sensor broken;
- hoặc đơn giản 20 phút không có train.

System không biết.

Nếu lecturer hỏi, m phải có một health-monitoring mechanism hoặc simply scope failure to:

> **sensor reports diagnostic fault / stuck state**

chứ đừng claim controller magically detects a silent failed train detector.

Không nhất thiết đưa cái này vào assumptions ngay, nhưng **t recommend chỉnh project spec**.

---

# 12. Có một choice t muốn m aware nhưng không bắt sửa

Railway reopening hiện dùng:

```text
45s estimated arrival
+
20s fixed crossing occupancy
→ reopen based on timer
```

và exit sensor chỉ future enhancement. 

M đã document limitation rất rõ nên logically okay.

Nhưng đây là **một trong những chỗ lecturer có khả năng attack nhất trong Q&A**, bởi vì project tự gọi railway system là “fail-safe”, trong khi:

> train stalls on crossing → timer expires → gates can reopen.

Spec cũng tự acknowledge limitation này. 

Nếu mục tiêu HD và implementation không quá khó, **exit sensor confirmation thật ra là feature t sẽ cân nhắc promote từ Optional → Core/HD** sau này.

Không phải bắt buộc sửa bây giờ. Nhưng safety story của railway sẽ mạnh hơn **rất nhiều** nếu reopening cũng sensor-confirmed giống gate-closing.

---

# Verdict cho `system_assumptions_tables.md`

**Structure:** ✅  
**Coverage against assignment:** ✅  
**Consistency with finalized project:** ~**90–95%**  
**Can copy straight into report right now:** **chưa**.

T sẽ sửa trước 5 việc:

1. **Delete Revision Note.**
2. Fix **PA-08**: retain timing/profile config nhưng mode được local clock tự select khi C1 down.
3. Fix **CC-03**: binary queue sensor → define bounded drain rule, không nói mơ hồ “size duration”.
4. Fix **PA-05**: normal presence sensor ≠ queue/congestion sensor.
5. Remove/update cross-references như `Safety Invariant #7`, `Section 5b` nếu report chưa có exact corresponding sections.

Còn `PROJECT_SPECIFICATION.md` thì t sẽ xem nó là **finalized baseline từ đây**, nhưng t đánh dấu 3 issue cần nhớ: **“C1 monitors all 10” typo**, **Central→railway high-level command ambiguity**, và **silent train-sensor failure detection**.

Nếu m muốn, **next turn t có thể đi thẳng vào từng subsection 1.2.1 → 1.2.7 và review kiểu “KEEP / MODIFY / DELETE” từng assumption**, để cuối cùng m có version sạch copy thẳng vào report.
