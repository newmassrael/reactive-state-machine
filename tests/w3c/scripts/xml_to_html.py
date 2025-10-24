#!/usr/bin/env python3
"""Convert W3C SCXML Test Results XML to HTML"""
import xml.etree.ElementTree as ET
import sys

def xml_to_html(xml_path, html_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()

    # Extract summary
    total = int(root.get('tests', 0))
    failures = int(root.get('failures', 0))
    errors = int(root.get('errors', 0))
    passed = total - failures - errors
    pass_rate = (passed / total * 100) if total > 0 else 0

    html = f'''<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>W3C SCXML Test Results</title>
<link href="https://unpkg.com/@primer/css@^20.2.4/dist/primer.css" rel="stylesheet" />
<style>
body{{background:#f6f8fa;padding:24px}}
.container{{max-width:1280px;margin:0 auto}}
.Box{{background:white;border:1px solid #d0d7de;border-radius:6px;margin-bottom:16px}}
.Box-header{{padding:16px;background:#f6f8fa;border-bottom:1px solid #d0d7de;border-radius:6px 6px 0 0}}
.Box-title{{font-size:14px;font-weight:600;color:#24292f}}
.Box-body{{padding:16px}}
.summary-grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:24px}}
.counter{{padding:16px;border:1px solid #d0d7de;border-radius:6px}}
.counter-label{{font-size:12px;color:#57606a;font-weight:600;text-transform:uppercase}}
.counter-value{{font-size:32px;font-weight:600;margin-top:8px}}
.counter.total{{border-left:3px solid #0969da}}
.counter.total .counter-value{{color:#0969da}}
.counter.pass{{border-left:3px solid #1a7f37}}
.counter.pass .counter-value{{color:#1a7f37}}
.counter.fail{{border-left:3px solid #cf222e}}
.counter.fail .counter-value{{color:#cf222e}}
.counter.error{{border-left:3px solid #fb8500}}
.counter.error .counter-value{{color:#fb8500}}
.pass-rate{{font-size:20px;font-weight:600;color:#1a7f37;margin-bottom:16px}}
.octicon{{display:inline-block;vertical-align:text-bottom;fill:currentColor}}
.State{{display:inline-block;padding:4px 12px;font-size:12px;font-weight:500;line-height:20px;border-radius:2em}}
.State--success{{color:#1a7f37;background-color:#dafbe1;border:1px solid #1a7f37}}
.State--failure{{color:#cf222e;background-color:#ffebe9;border:1px solid #cf222e}}
.State--error{{color:#9a6700;background-color:#fff8c5;border:1px solid #d4a72c}}
.Label{{display:inline-block;padding:2px 7px;font-size:12px;font-weight:500;line-height:18px;border-radius:2em}}
.Label--primary{{color:#0969da;background-color:#ddf4ff;border:1px solid #0969da}}
.Label--success{{color:#1a7f37;background-color:#dafbe1;border:1px solid #1a7f37}}
.Label--warning{{color:#9a6700;background-color:#fff8c5;border:1px solid #d4a72c}}
.Label--danger{{color:#cf222e;background-color:#ffebe9;border:1px solid #cf222e}}
table{{width:100%;border-collapse:collapse}}
th{{padding:8px 16px;font-size:12px;font-weight:600;color:#57606a;text-align:left;background:#f6f8fa;border-bottom:1px solid #d0d7de}}
td{{padding:8px 16px;font-size:14px;border-bottom:1px solid #d0d7de}}
tr:last-child td{{border-bottom:0}}
.test-name{{font-weight:600;color:#24292f}}
.time-value{{font-family:ui-monospace,SFMono-Regular,SF Mono,Menlo,Consolas,Liberation Mono,monospace;font-size:12px;color:#57606a}}
.flash{{padding:16px;margin-bottom:16px;border:1px solid transparent;border-radius:6px}}
.flash-error{{color:#cf222e;background-color:#ffebe9;border-color:#cf8f91}}
</style>
</head>
<body>
<div class="container">
<div class="Box">
<div class="Box-header">
<h3 class="Box-title">
<svg class="octicon" xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
<path d="M8 0a8 8 0 1 1 0 16A8 8 0 0 1 8 0ZM1.5 8a6.5 6.5 0 1 0 13 0 6.5 6.5 0 0 0-13 0Zm9.78-2.22-5.5 5.5a.75.75 0 0 1-1.06 0l-2.5-2.5a.749.749 0 0 1 .326-1.275.749.749 0 0 1 .734.215L5 9.44l4.97-4.97a.749.749 0 0 1 1.275.326.749.749 0 0 1-.215.734Z"></path>
</svg>
W3C SCXML Compliance Test Results
</h3>
</div>
<div class="Box-body">
<div class="summary-grid">
<div class="counter total">
<div class="counter-label">Total Tests</div>
<div class="counter-value">{total}</div>
</div>
<div class="counter pass">
<div class="counter-label">Passed</div>
<div class="counter-value">{passed}</div>
</div>
<div class="counter fail">
<div class="counter-label">Failed</div>
<div class="counter-value">{failures}</div>
</div>
<div class="counter error">
<div class="counter-label">Errors</div>
<div class="counter-value">{errors}</div>
</div>
</div>
<div class="pass-rate">
<svg class="octicon" xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
<path d="M13.78 4.22a.75.75 0 0 1 0 1.06l-7.25 7.25a.75.75 0 0 1-1.06 0L2.22 9.28a.751.751 0 0 1 .018-1.042.751.751 0 0 1 1.042-.018L6 10.94l6.72-6.72a.75.75 0 0 1 1.06 0Z"></path>
</svg>
Pass Rate: {pass_rate:.1f}%
</div>
</div>
</div>
'''

    # Process each testsuite
    for testsuite in root.findall('testsuite'):
        suite_name = testsuite.get('name', '')
        test_count = testsuite.get('tests', 0)

        html += f'''
<div class="Box">
<div class="Box-header">
<h3 class="Box-title">{suite_name}</h3>
<span class="Counter">{test_count}</span>
</div>
<div class="Box-body" style="padding:0">
<table>
<thead>
<tr>
<th>Test</th>
<th>Status</th>
<th>Type</th>
<th>Time</th>
<th>Description</th>
</tr>
</thead>
<tbody>
'''

        for testcase in testsuite.findall('testcase'):
            test_name = testcase.get('name', '')
            time_val = testcase.get('time', '0')
            test_type = testcase.get('type', '')
            result = testcase.get('result', 'PASS')
            description = testcase.get('description', '')

            # GitHub-style icons
            if result == 'PASS':
                icon = '''<svg class="octicon" style="color:#1a7f37" xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
<path d="M13.78 4.22a.75.75 0 0 1 0 1.06l-7.25 7.25a.75.75 0 0 1-1.06 0L2.22 9.28a.751.751 0 0 1 .018-1.042.751.751 0 0 1 1.042-.018L6 10.94l6.72-6.72a.75.75 0 0 1 1.06 0Z"></path>
</svg>'''
                state_class = 'State--success'
            elif result == 'FAIL':
                icon = '''<svg class="octicon" style="color:#cf222e" xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
<path d="M3.72 3.72a.75.75 0 0 1 1.06 0L8 6.94l3.22-3.22a.749.749 0 0 1 1.275.326.749.749 0 0 1-.215.734L9.06 8l3.22 3.22a.749.749 0 0 1-.326 1.275.749.749 0 0 1-.734-.215L8 9.06l-3.22 3.22a.751.751 0 0 1-1.042-.018.751.751 0 0 1-.018-1.042L6.94 8 3.72 4.78a.75.75 0 0 1 0-1.06Z"></path>
</svg>'''
                state_class = 'State--failure'
            else:
                icon = '''<svg class="octicon" style="color:#9a6700" xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
<path d="M6.457 1.047c.659-1.234 2.427-1.234 3.086 0l6.082 11.378A1.75 1.75 0 0 1 14.082 15H1.918a1.75 1.75 0 0 1-1.543-2.575Zm1.763.707a.25.25 0 0 0-.44 0L1.698 13.132a.25.25 0 0 0 .22.368h12.164a.25.25 0 0 0 .22-.368Zm.53 3.996v2.5a.75.75 0 0 1-1.5 0v-2.5a.75.75 0 0 1 1.5 0ZM9 11a1 1 0 1 1-2 0 1 1 0 0 1 2 0Z"></path>
</svg>'''
                state_class = 'State--error'

            html += f'''
<tr>
<td><span class="test-name">{test_name}</span></td>
<td>
{icon}
<span class="State {state_class}">{result}</span>
</td>
<td>{test_type}</td>
<td><span class="time-value">{time_val}s</span></td>
<td>{description if description else '-'}</td>
</tr>
'''

        html += '''
</tbody>
</table>
</div>
</div>
'''

    html += '</div>\n</body>\n</html>\n'

    with open(html_path, 'w', encoding='utf-8') as f:
        f.write(html)

    print(f"HTML report generated: {html_path}")

if __name__ == '__main__':
    xml_path = 'w3c_test_results.xml' if len(sys.argv) < 2 else sys.argv[1]
    html_path = xml_path.replace('.xml', '.html')
    xml_to_html(xml_path, html_path)
