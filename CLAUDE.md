Hãy **độc lập kiểm tra lại** 7 test plan trong `/Users/lucietran/Documents/GitHub/traffic-control-system/docs/test-plan/` so với source hiện tại trong `app/`. Đây là bước rà soát trước khi viết Section 8 của Implementation Note. **Đừng sửa file và đừng giả định test đã chạy hoặc Pass chỉ vì test plan ghi expected result.** Bản triển khai demo dùng 10 QNX VM, mỗi controller một VM; project Momentics thực tế có thể dùng tên binary khác tên source entry point.

Kiểm tra kỹ các nghi vấn sau:

1. Các case tìm log `SIGNAL -> ...` có còn khớp output thực tế của `lx_signal.c` không? Tìm thêm những chuỗi output cũ tương tự.
2. Test plan dùng topology 3 VM và lệnh `./c_main`, `./lx_main`, `./rlx_main`: phần nào phải đổi để áp dụng cho demo 10 VM? Tên binary nào có thể xác nhận từ repo, tên nào phải xác nhận trên QNX?
3. `04-timing-assumptions.md` nói tự chuyển Peak/Off-Peak chưa chạy. Đối chiếu `c_main.c`, `c_operator.c` để xác định hành vi hiện tại. Kiểm tra các bảng phím thiếu `d/a` ở C1 và `r` ở RLx; xem việc này ảnh hưởng case fault-clear nào.
4. `TC-TIME-05` có thật sự gửi được offset 89999/90000 bằng phím `t` không? Liệt kê các case cần `test_client` hoặc debugger và xác nhận những công cụ đó có tồn tại trong repo không.
5. Các bước dùng `central_log.txt`, HMI hoặc đồng hồ bấm giờ có đủ chứng minh heartbeat 1 Hz, ACK `<1 s`, và các biên 100 ms không? Đối chiếu độ phân giải timestamp và cột HMI thực tế.
6. Đối chiếu cách dùng `kill -STOP` để kích watchdog trong file 02 với giải thích ở file 05. Nêu kết luận nào có thể xác nhận từ code và kết luận nào cần chạy thử.
7. Với các case khởi động node theo thứ tự bất kỳ trong file 07, kiểm tra RLx có gửi lại crossing status ban đầu cho Lx khởi động muộn không. Phân biệt “heartbeat hồi phục” với “đã đồng bộ đủ crossing state”.
8. Kiểm tra tổng case: README ghi 232, phụ lục file timing ghi 31 dù danh sách có vẻ là 32, còn `docs/implementation-note/section-8-verification.md` ghi 227 case đều Pass. Xác định con số đúng theo **ID test duy nhất**, và xem có bằng chứng thực thi nào hỗ trợ lời khẳng định “all passed” không.

Với **mỗi phát hiện**, cho: file và dòng cụ thể, trích ngắn expected/test step, hành vi source hiện tại, mức độ chắc chắn, cách sửa test plan hoặc cách kiểm chứng trên QNX. Nếu nghi vấn nào của tôi sai thì nói rõ và đưa bằng chứng phản bác. Cuối cùng, tách riêng: **(a) test có thể chạy bằng bàn phím hiện tại, (b) cần công cụ/debugger, (c) cần đo hoặc quan sát bổ sung**. Chưa viết Section 8 và chưa gán Pass/Fail khi không có observed result.