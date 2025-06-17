import pytesseract
from PIL import Image
import csv
import os

# Path to your image file
image_path = 'IMG_1139.jpg'
output_txt = 'IMG_1139.txt'
output_csv = 'data_id_codes.csv'

def extract_text(image_path, output_txt):
    text = pytesseract.image_to_string(Image.open(image_path))
    with open(output_txt, 'w') as f:
        f.write(text)
    print(f"Extracted text saved to {output_txt}")
    return text

def text_to_csv(text, output_csv):
    # Split text into lines and try to split by whitespace or tabs
    lines = [line.strip() for line in text.split('\n') if line.strip()]
    rows = []
    for line in lines:
        # Try splitting by tab, then by multiple spaces
        if '\t' in line:
            row = line.split('\t')
        else:
            row = [col for col in line.split('  ') if col]
        rows.append(row)
    # Write to CSV
    with open(output_csv, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerows(rows)
    print(f"CSV saved to {output_csv}")

def main():
    if not os.path.exists(image_path):
        print(f"Image file {image_path} not found.")
        return
    text = extract_text(image_path, output_txt)
    text_to_csv(text, output_csv)

if __name__ == '__main__':
    main()
