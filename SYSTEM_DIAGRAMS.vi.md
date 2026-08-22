# Sơ Đồ Hệ Thống (ASCII) — Bản Tiếng Việt

Đi kèm [`PROJECT_SPECIFICATION.vi.md`](PROJECT_SPECIFICATION.vi.md). Tên component và ký hiệu trạng thái giữ nguyên tiếng Anh.

Tất cả sơ đồ dưới đây dùng **ký tự ASCII thuần** (`=`, `-`, `|`, `+`, `#`) thay vì ký tự Unicode vẽ khung (═, │, ▓...) — lý do: nhiều ký tự Unicode vẽ khung/mũi tên bị hiển thị *rộng gấp đôi* trên 1 số font/trình xem tiếng Việt, khiến các cột bị lệch dù mã nguồn đã canh đúng. ASCII thuần đảm bảo canh thẳng hàng ở mọi nơi.

Nhắc lại quy ước đặt tên: `I1–I6` / `RC1–RC3` là vị trí **vật lý**; `L1–L6` / `RL1–RL3` là **controller** sở hữu chúng; `C1` là Central Controller.

---

## Diagram 1 — Tổng quan mạng lưới

```text
LEGEND
  ====   duong arterial (R1/R2), huong Tay-Dong
  |      duong connector (R3/R4/R5), huong Bac-Nam
  ####   hanh lang duong sat doi, huong Tay-Dong
  [In]   giao lo (controller Ln)      (RCn) cho giao duong sat (controller RLn)

                                   BAC (North)

           R1
      ==[I1]============[I3]============[I5]==
          |                 |                |
          | R3              | R4             | R5
          |                 |                |
      ##########################################
        (RC1/RL1)       (RC2/RL2)        (RC3/RL3)
          |                 |                |
          |                 |                |
      ==[I2]============[I4]============[I6]==
           R2

                                   NAM (South)
```

---

## Diagram 2 — Làn đường Core (2 làn, permissive, đi bên phải)

```text
Duong connector R3 (Bac-Nam giua I1 va I2), nhin tu tren xuong.
Viet Nam di ben PHAI: dai phan cach luon o ben TRAI huong di.

           CHIEU BAC          |          CHIEU NAM
   -----------------------    |    -----------------------
   lan trong: thang + trai    |    lan trong: thang + trai
   -----------------------    |    -----------------------
   lan ngoai: thang + phai    |    lan ngoai: thang + phai
   -----------------------    |    -----------------------
                     (dai phan cach o giua)

Quy tac Core:
  - lan trong (sat dai phan cach) = di thang + re trai (permissive)
  - lan ngoai (sat le duong)      = di thang + re phai (permissive)
  - 1 den tron / approach - KHONG co den mui ten rieng
```

---

## Diagram 3 — Bố cục component tại 1 giao lộ (`Ix` / `Lx` tổng quát)

```text
LEGEND
  [V] dau den tin hieu xe        [Ps] den tin hieu nguoi di bo
  [Pb] nut bam nguoi di bo       [S]  sensor phat hien xe
  [Q]  advance/queue sensor (chi co neu approach nay dan toi 1 railway crossing)

                        BAC (North)
                  [V][S]     [Pb][Ps]


  TAY (West)                                    DONG (East)
  [V][S]                                            [S][V]
  [Pb][Ps]        +-----------------+          [Pb][Ps]
                   |                 |
                   |       Lx        |<--- status/lenh --->  C1
                   |                 |
                   +-----------------+


                  [V][S]     [Pb][Ps]
                  [Q]  (chi co o approach dan toi
                        1 railway crossing, vd
                        approach Nam cua I1 -> RC1)
                        NAM (South)
```

Ghi chú: `Lx` sở hữu và điều khiển tất cả thiết bị `[V]/[Pb]/[Ps]/[S]/[Q]` ở cả 4 approach. Nếu 1 approach dẫn tới railway crossing, `Lx` còn nhận status (1 chiều, chỉ đọc) từ `RLx` — xem Diagram 4.

---

## Diagram 4 — Chỗ giao đường sắt + 2 giao lộ liền kề (`RCx` / `RLx` tổng quát)

Ví dụ `RC1` trên đường `R3`, giữa `I1` (Bắc, controller `L1`) và `I2` (Nam, controller `L2`) — áp dụng y hệt cho `RC2` (giữa `I3`/`I4`) và `RC3` (giữa `I5`/`I6`).

```text
LEGEND
  [FL] den nhap nhay        [BG]  boom gate       [TA] sensor tiep can tau
  [BgS] sensor vi tri gate  [TS]  train signal     [Ex] exit sensor (de danh, khong dung o Core)

        I1 (L1)  -- phia Bac
           |
           |   R3 - duong connector, 2 lan moi chieu
           |
   [TA-A]--+--[BG-Bac][BgS-Bac][FL-Bac][TS-A]      <- huong ray A
           |
      #########  RC1 - hanh lang duong sat doi  #########
           |
   [TA-B]--+--[BG-Nam][BgS-Nam][FL-Nam][TS-B]      <- huong ray B
           |
        I2 (L2)  -- phia Nam


              +--------+
              |  RL1   |-------- status/loi -------->  C1
              +--------+
       so huu & dieu khien: [BG], [FL], [TS] o ca 2 huong
       gui trang thai crossing (CHI DOC, khong lenh) toi ca L1 va L2
```

Quy tắc an toàn thể hiện ở đây (xem `PROJECT_SPECIFICATION.vi.md`, Mục 22):

- `RL1` là controller **duy nhất** được điều khiển `[BG]`, `[FL]`, `[TS]`.
- `L1`/`L2` chỉ được **đọc** trạng thái `RC1` — không bao giờ ra lệnh.
- `[TS]` chỉ hiện `PROCEED` khi cả 2 `[BgS]` xác nhận gate đã hạ hết (`DOWN`).
- `[Ex]` tồn tại vật lý nhưng chưa nối vào quyết định mở gate — Core dùng timer cửa sổ chiếm dụng cố định (Mục 14).
