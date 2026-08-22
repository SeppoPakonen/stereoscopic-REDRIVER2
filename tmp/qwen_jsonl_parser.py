import sys, json

def parse_qwen_jsonl(path):
    entries = []
    with open(path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
                entries.append(obj)
            except json.JSONDecodeError:
                continue
    return entries

def main():
    if len(sys.argv) != 2:
        print("Usage: python qwen_jsonl_parser.py <path_to_jsonl>")
        return
    path = sys.argv[1]
    entries = parse_qwen_jsonl(path)
    
    # Build summary: count of messages per role
    role_counts = {}
    last_user_msg = None
    for entry in entries:
        if entry.get('type') == 'user':
            parts = entry.get('message', {}).get('parts', [])
            if parts:
                last_user_msg = parts[0].get('text', '')
        role = entry.get('provenance', 'unknown')
        role_counts[role] = role_counts.get(role, 0) + 1
    
    # Output
    print('=== SUMMARY ===')
    for role, count in role_counts.items():
        print(f'{role}: {count}')
    print('=== TAIL CONVERSATION ===')
    if last_user_msg:
        print(last_user_msg)

if __name__ == '__main__':
    main()