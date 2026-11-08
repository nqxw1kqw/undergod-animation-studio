# Bộ asset hiện tại — cinematic phóng lao

50 PNG runtime: 24 frame Gà Chiến Binh, 20 frame Ếch Vương, 4 icon thẻ, một cây lao và một sheet va chạm. Không dùng sheet ngôi sao cũ.

- Nhân vật: frame 512×512, chân y=456. Gà có idle/attack/hit/defend/ult_charge/ult_throw; boss có năm động tác cũ. Mỗi động tác 4 frame.
- ult_throw phát 4 fps của đồng hồ gameplay; hai frame cuối tay trống, không vẽ lại giáo đang bay. Giữ pose tới hết cinematic rồi trở về idle.
- vfx/spear.png: PNG alpha 512×512, mũi hướng lên, tip tại (256,56). Renderer đặt tip theo quỹ đạo, xoay 180 độ khi rơi. Ảnh không có nhân vật hoặc trail dính sẵn.
- vfx/spear_impact.png: PNG alpha 512×512 gồm 2×2 cell 256×256, phát 8 fps. Gồm contact spark, shock flash và bụi tan; không có ngôi sao.
- Bốn icon thẻ vẫn là giáo, khiên gỗ, đâm mạnh và bánh quy, mỗi ảnh 256×256.

Đồng hồ x1 chạy 0,65 thời gian thực; x2/x3 nhân đồng hồ tương ứng nên sprite, di chuyển, projectile, popup và camera đồng bộ. HUD/đường nền vẫn vẽ bằng code.

Ảnh gốc, prompt và công cụ hậu xử lý được lưu riêng tại máy phát triển. Manifest SHA-256 hiện dùng ở assets/manifest.json, tính từ gốc project UndergodCard. Game không cần Python hoặc công cụ sinh ảnh để build/chạy.
