import hashlib

def file_hash(path, algo="sha256"):
    h = hashlib.new(algo)
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()

original = "test.iso"
decoded = "result.iso"

hash_orig = file_hash(original)
hash_dec = file_hash(decoded)

print("Original hash:", hash_orig)
print("Decoded  hash:", hash_dec)

if hash_orig == hash_dec:
    print("✅ Files are identical!")
else:
    print("❌ Files are different!")

