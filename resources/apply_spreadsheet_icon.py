import os
from PIL import Image

def update_icons():
    src_ico = r"C:\Users\Jun\Documents\antigravity\epic-nobel\resources\spreadsheet_icon.ico"
    if not os.path.exists(src_ico):
        print("Error: spreadsheet_icon.ico not found!")
        return
        
    with Image.open(src_ico) as img:
        img = img.convert("RGBA")
        print("Original source ICO size:", img.size)
        
        # 128x128 원본을 256x256 최고 화질(Lanczos)로 업스케일하여 바탕화면 큰 아이콘 모드에서도 꽉 차게 렌더링되도록 함
        img_256 = img.resize((256, 256), Image.Resampling.LANCZOS)
        
        out_dir = r"C:\Users\Jun\Documents\antigravity\epic-nobel\resources"
        
        # 1. lightcell.png 저장
        png_path = os.path.join(out_dir, "lightcell.png")
        img_256.save(png_path, "PNG")
        print("Saved:", png_path)
        
        # 2. lightcell_minimal.png 저장
        min_png_path = os.path.join(out_dir, "lightcell_minimal.png")
        img_256.save(min_png_path, "PNG")
        print("Saved:", min_png_path)
        
        # 3. 멀티 사이즈 ICO 생성 (256x256 포함하여 바탕화면 꽉 채움)
        sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
        
        ico_path = os.path.join(out_dir, "lightcell.ico")
        img_256.save(ico_path, format="ICO", sizes=sizes)
        print("Saved multi-res ICO:", ico_path)
        
        min_ico_path = os.path.join(out_dir, "lightcell_minimal.ico")
        img_256.save(min_ico_path, format="ICO", sizes=sizes)
        print("Saved multi-res minimal ICO:", min_ico_path)

if __name__ == "__main__":
    update_icons()
