import os

from PIL import Image


def slice_ui_image(image_path, output_dir="output_slices"):
    # Target 1080p dimensions
    TARGET_W, TARGET_H = 1920, 1080

    # Open the image safely
    if not os.path.exists(image_path):
        print(f"Error: File '{image_path}' not found.")
        return

    with Image.open(image_path) as img:
        img_w, img_h = img.size
        print(f"Loaded image size: {img_w}x{img_h}")

        # Verify if the original image is large enough
        if img_w < TARGET_W or img_h < TARGET_H:
            print(
                f"Error: Image size ({img_w}x{img_h}) is smaller than the target 1080p viewport."
            )
            return

        # Create output folder if it doesn't exist
        os.makedirs(output_dir, exist_ok=True)

        # Calculate horizontal centering boundaries
        left = (img_w - TARGET_W) // 2
        right = left + TARGET_W

        # 1. Top Centered 1080p
        top_slice = (left, 0, right, TARGET_H)

        # 2. True Center 1080p
        center_top = (img_h - TARGET_H) // 2
        center_slice = (left, center_top, right, center_top + TARGET_H)

        # 3. Bottom Centered 1080p
        bottom_top = img_h - TARGET_H
        bottom_slice = (left, bottom_top, right, img_h)

        # Crop and save configurations
        slices = {
            "top_1080p.png": top_slice,
            "center_1080p.png": center_slice,
            "bottom_1080p.png": bottom_slice,
        }

        for name, box in slices.items():
            cropped_img = img.crop(box)
            save_path = os.path.join(output_dir, name)
            cropped_img.save(save_path, "PNG")  # PNG ensures pixel-perfect compression
            print(f"Saved: {save_path} (Bounding Box: {box})")


if __name__ == "__main__":
    # Replace with your actual large image filename
    YOUR_IMAGE_FILE = "test_newurban.png"
    slice_ui_image(YOUR_IMAGE_FILE)
