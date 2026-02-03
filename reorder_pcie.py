import re

path = 'os/linux/hif/pcie/pcie.c'
with open(path, 'r') as f:
    lines = f.readlines()

# 1. Capture the globals and headers
header_end_idx = 0
for i, line in enumerate(lines):
    if 'static u_int8_t g_fgDriverProbed' in line:
        header_end_idx = i + 2
        break

header = lines[:header_end_idx]
body = lines[header_end_idx:]

# 2. Extract our custom recovery logic to move it to the top
logic_pattern = re.compile(r'(static int mt7902_mmio_dead.*?|void mt7902_schedule_recovery_from_atomic.*?|static void mt7902_recovery_work.*?)\n(?=static|void|uint32_t|/\*)', re.DOTALL)
content_str = "".join(body)

# Helper to find and extract blocks
def extract_block(name, text):
    start = text.find(name)
    if start == -1: return "", text
    # Find the closing brace of the function
    brace_count = 0
    started = False
    for i in range(start, len(text)):
        if text[i] == '{':
            brace_count += 1
            started = True
        elif text[i] == '}':
            brace_count -= 1
        if started and brace_count == 0:
            block = text[start:i+1]
            return block, text[:start] + text[i+1:]
    return "", text

dead_block, content_str = extract_block('static int mt7902_mmio_dead', content_str)
sched_block, content_str = extract_block('void mt7902_schedule_recovery_from_atomic', content_str)
work_block, content_str = extract_block('static void mt7902_recovery_work', content_str)

# 3. Reconstruct with the missing Global CSRBaseAddress
new_header = "".join(header)
if 'static void *CSRBaseAddress;' not in new_header:
    new_header += "\nstatic void *CSRBaseAddress;\n"

# 4. Re-order the functions so they are defined before use
reordered_content = (
    new_header + "\n" +
    dead_block + "\n\n" +
    sched_block + "\n\n" +
    work_block + "\n\n" +
    content_str
)

with open(path, 'w') as f:
    f.write(reordered_content)
