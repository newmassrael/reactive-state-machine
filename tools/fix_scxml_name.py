#!/usr/bin/env python3
"""
Fix SCXML file name attribute - remove name from first <scxml> and add test name
Usage: fix_scxml_name.py <scxml_file> <test_name>
"""
import sys
import re

def fix_scxml_name(scxml_path, test_name):
    with open(scxml_path, 'r') as f:
        content = f.read()

    # Find first <scxml> tag (up to '>') and remove any name attribute
    first_scxml_match = re.search(r'<scxml[^>]*?>', content, flags=re.DOTALL)
    if not first_scxml_match:
        return  # No <scxml> tag found

    first_scxml = first_scxml_match.group(0)
    start_pos = first_scxml_match.start()
    end_pos = first_scxml_match.end()

    # Remove name attribute from first <scxml> tag
    fixed_scxml = re.sub(r'\s+name="[^"]*"', '', first_scxml)

    # Add new name attribute after <scxml
    fixed_scxml = fixed_scxml.replace('<scxml', f'<scxml name="{test_name}"', 1)

    # Replace first <scxml> tag in content
    content = content[:start_pos] + fixed_scxml + content[end_pos:]

    with open(scxml_path, 'w') as f:
        f.write(content)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: fix_scxml_name.py <scxml_file> <test_name>")
        sys.exit(1)

    fix_scxml_name(sys.argv[1], sys.argv[2])
