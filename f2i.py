from PIL import Image
import math

def file_to_image(input_file, output_png):
    with open(input_file, "rb") as f:
        data = f.read()

    bits = []
    for byte in data:
        for i in range(8):
            bits.append((byte >> (7 - i)) & 1)

    total_bits = len(bits)
    side = math.ceil(math.sqrt(total_bits))

    bits += [0] * (side * side - total_bits)

    img = Image.new("1", (side, side))
    img.putdata(bits)
    img.save(output_png)

    print(f"Saved {output_png}, size: {side}×{side} pixels")


if __name__ == "__main__":
    file = input("Enter file path: ")
    out = input("Enter output PNG name: ")
    file_to_image(file, out)

