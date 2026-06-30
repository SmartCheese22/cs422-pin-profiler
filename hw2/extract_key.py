import sys
from collections import Counter

def extract_key(trace_file):
    try:
        with open(trace_file, "r") as f:
            lines = [line.strip().split() for line in f if line.strip()]
    except Exception as e:
        print(f"[!] Error reading trace: {e}")
        return

    if not lines:
        print("[!] Trace file is empty.")
        return

    # 1. Isolate the RSA loop
    # Find the single Instruction Pointer (IP) that executes the most.
    # This cleanly filters out printf and libc noise!
    ips = [parts[0] for parts in lines]
    most_common_ip, count = Counter(ips).most_common(1)[0]
    
    # Filter the trace to ONLY include this specific instruction
    rsa_icounts = [int(parts[1]) for parts in lines if parts[0] == most_common_ip]
    
    print(f"[*] Identified RSA Core loop at IP: {most_common_ip} ({count} executions)")

    # 2. Calculate the distances between the RSA multiplications
    deltas = [rsa_icounts[i+1] - rsa_icounts[i] for i in range(len(rsa_icounts)-1)]
    
    if not deltas:
        print("[*] Only one multiplication found. Key must be 0.")
        with open("key.txt", "w") as f: f.write("0\n")
        return

    # 3. Determine threshold (Short gap = inside bit 1, Long gap = next loop)
    max_d = max(deltas)
    min_d = min(deltas)
    
    if max_d - min_d < 20:
        # All gaps are exactly the same size, meaning it's all 0s
        threshold = -1 
    else:
        # Set a threshold comfortably between the short gap and long gap
        threshold = min_d + (max_d - min_d) / 3

    # 4. Decode the bits
    bits = []
    i = 0
    while i < len(rsa_icounts):
        # If the next multiplication happens very quickly, it's the "Multiply" step of a '1' bit
        if i + 1 < len(rsa_icounts) and (rsa_icounts[i+1] - rsa_icounts[i]) < threshold:
            bits.append("1")
            i += 2  # Consume both the Square and the Multiply
        else:
            # Otherwise, it was just a Square step of a '0' bit
            bits.append("0")
            i += 1  # Consume only the Square

    binary_key = "".join(bits)
    
    print(f"[*] Total bits extracted : {len(binary_key)}")
    print(f"[*] Extracted Binary Key : {binary_key}")
    
    try:
        decimal_key = int(binary_key, 2)
        print(f"[*] Extracted Decimal Key: {decimal_key}")
        with open("key.txt", "w") as f:
            f.write(str(decimal_key) + "\n")
        print("[*] Successfully saved to key.txt")
    except ValueError:
        print("[!] Error parsing the key.")

if __name__ == "__main__":
    extract_key("mul_trace.txt")
