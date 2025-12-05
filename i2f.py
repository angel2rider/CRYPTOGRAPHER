from PIL import Image

def image_to_file(input_png, output_file):
    img = Image.open(input_png).convert("1")
    raw_bits = list(img.getdata())

    # Convert 255 -> 1 and 0 -> 0
    bits = [1 if b > 0 else 0 for b in raw_bits]

    data = bytearray()
    for i in range(0, len(bits), 8):
        byte_bits = bits[i:i+8]
        if len(byte_bits) < 8:
            break
        byte = 0
        for b in byte_bits:
            byte = (byte << 1) | b
        data.append(byte)

    with open(output_file, "wb") as f:
        f.write(data)

    print(f"Recovered file saved as {output_file}")


if __name__ == "__main__":
    inp = input("Enter PNG file: ")
    out = input("Enter output file: ")
    image_to_file(inp, out)

