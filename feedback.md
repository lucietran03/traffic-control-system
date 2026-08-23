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
2. Fix **PA-07**: retain timing/profile config nhưng mode được local clock tự select khi C1 down.
3. Fix **CC-03**: binary queue sensor → define bounded drain rule, không nói mơ hồ “size duration”.
4. Fix **PA-04**: normal presence sensor ≠ queue/congestion sensor.
5. Remove/update cross-references như `Safety Invariant #7`, `Section 5b` nếu report chưa có exact corresponding sections.

Còn `PROJECT_SPECIFICATION.md` thì t sẽ xem nó là **finalized baseline từ đây**, nhưng t đánh dấu 3 issue cần nhớ: **“C1 monitors all 10” typo**, **Central→railway high-level command ambiguity**, và **silent train-sensor failure detection**.

Nếu m muốn, **next turn t có thể đi thẳng vào từng subsection 1.2.1 → 1.2.7 và review kiểu “KEEP / MODIFY / DELETE” từng assumption**, để cuối cùng m có version sạch copy thẳng vào report.
