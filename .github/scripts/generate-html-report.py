#!/usr/bin/env python3
"""
Generate HTML test report from JUnit XML
"""
import xml.etree.ElementTree as ET
import sys
from datetime import datetime

def parse_junit_xml(xml_path):
    """Parse JUnit XML file and extract test results"""
    tree = ET.parse(xml_path)
    root = tree.getroot()

    # Extract summary from testsuites
    total_tests = int(root.get('tests', 0))
    total_failures = int(root.get('failures', 0))
    total_errors = int(root.get('errors', 0))
    total_time = float(root.get('time', 0))

    # Extract testsuites
    testsuites = []
    for testsuite in root.findall('testsuite'):
        suite_name = testsuite.get('name')
        suite_tests = int(testsuite.get('tests', 0))
        suite_failures = int(testsuite.get('failures', 0))
        suite_errors = int(testsuite.get('errors', 0))
        suite_time = float(testsuite.get('time', 0))

        # Extract testcases
        testcases = []
        for testcase in testsuite.findall('testcase'):
            tc_name = testcase.get('name')
            tc_classname = testcase.get('classname')
            tc_time = float(testcase.get('time', 0))

            # Check for failures
            failure = testcase.find('failure')
            if failure is not None:
                tc_status = 'failed'
                tc_message = failure.get('message', '')
            else:
                tc_status = 'passed'
                tc_message = ''

            testcases.append({
                'name': tc_name,
                'classname': tc_classname,
                'time': tc_time,
                'status': tc_status,
                'message': tc_message
            })

        testsuites.append({
            'name': suite_name,
            'tests': suite_tests,
            'failures': suite_failures,
            'errors': suite_errors,
            'time': suite_time,
            'testcases': testcases
        })

    return {
        'total_tests': total_tests,
        'total_failures': total_failures,
        'total_errors': total_errors,
        'total_passed': total_tests - total_failures - total_errors,
        'total_time': total_time,
        'testsuites': testsuites
    }

def generate_html(data):
    """Generate HTML report from parsed data"""
    pass_rate = (data['total_passed'] / data['total_tests'] * 100) if data['total_tests'] > 0 else 0

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>W3C SCXML Test Results</title>
    <style>
        * {{
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }}
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: #f6f8fa;
            color: #24292e;
            padding: 10px;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 6px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.12);
            padding: 20px;
        }}
        @media (min-width: 768px) {{
            body {{
                padding: 20px;
            }}
            .container {{
                padding: 30px;
            }}
        }}
        h1 {{
            color: #24292e;
            margin-bottom: 10px;
            font-size: 24px;
            word-break: break-word;
        }}
        @media (min-width: 768px) {{
            h1 {{
                font-size: 32px;
            }}
        }}
        .timestamp {{
            color: #586069;
            font-size: 12px;
            margin-bottom: 20px;
        }}
        @media (min-width: 768px) {{
            .timestamp {{
                font-size: 14px;
                margin-bottom: 30px;
            }}
        }}
        .summary {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
            gap: 10px;
            margin-bottom: 20px;
        }}
        @media (min-width: 768px) {{
            .summary {{
                grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
                gap: 20px;
                margin-bottom: 30px;
            }}
        }}
        .summary-card {{
            padding: 15px;
            border-radius: 6px;
            border: 1px solid #e1e4e8;
        }}
        @media (min-width: 768px) {{
            .summary-card {{
                padding: 20px;
            }}
        }}
        .summary-card.total {{
            background: #f6f8fa;
        }}
        .summary-card.passed {{
            background: #dcffe4;
            border-color: #34d058;
        }}
        .summary-card.failed {{
            background: #ffdce0;
            border-color: #d73a49;
        }}
        .summary-card h3 {{
            font-size: 11px;
            color: #586069;
            margin-bottom: 6px;
            text-transform: uppercase;
        }}
        @media (min-width: 768px) {{
            .summary-card h3 {{
                font-size: 14px;
                margin-bottom: 8px;
            }}
        }}
        .summary-card .value {{
            font-size: 28px;
            font-weight: 600;
        }}
        @media (min-width: 768px) {{
            .summary-card .value {{
                font-size: 36px;
            }}
        }}
        .pass-rate {{
            font-size: 14px;
            color: #586069;
            margin-top: 5px;
        }}
        @media (min-width: 768px) {{
            .pass-rate {{
                font-size: 18px;
            }}
        }}
        .testsuite {{
            margin-bottom: 30px;
        }}
        @media (min-width: 768px) {{
            .testsuite {{
                margin-bottom: 40px;
            }}
        }}
        .testsuite-header {{
            background: #f6f8fa;
            padding: 12px 15px;
            border-radius: 6px 6px 0 0;
            border: 1px solid #e1e4e8;
            cursor: pointer;
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: 10px;
        }}
        @media (min-width: 768px) {{
            .testsuite-header {{
                padding: 15px 20px;
                flex-wrap: nowrap;
            }}
        }}
        .testsuite-header:hover {{
            background: #f1f3f5;
        }}
        .testsuite-title {{
            font-size: 16px;
            font-weight: 600;
            flex: 1 1 100%;
        }}
        @media (min-width: 768px) {{
            .testsuite-title {{
                font-size: 20px;
                flex: 1 1 auto;
            }}
        }}
        .testsuite-stats {{
            display: flex;
            gap: 10px;
            font-size: 12px;
            flex-wrap: wrap;
        }}
        @media (min-width: 768px) {{
            .testsuite-stats {{
                gap: 20px;
                font-size: 14px;
            }}
        }}
        .stat-passed {{
            color: #28a745;
        }}
        .stat-failed {{
            color: #d73a49;
        }}
        .testsuite-content {{
            border: 1px solid #e1e4e8;
            border-top: none;
            border-radius: 0 0 6px 6px;
            overflow-x: auto;
        }}
        .testsuite-content.collapsed {{
            display: none;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            min-width: 600px;
        }}
        th {{
            width: 60%;
        }}
        th:nth-child(2) {{
            width: 20%;
        }}
        th:nth-child(3) {{
            width: 20%;
        }}
        th {{
            text-align: left;
            padding: 10px 12px;
            background: #fafbfc;
            font-weight: 600;
            font-size: 11px;
            color: #586069;
            text-transform: uppercase;
            border-bottom: 1px solid #e1e4e8;
        }}
        @media (min-width: 768px) {{
            th {{
                padding: 12px 20px;
                font-size: 12px;
            }}
        }}
        td {{
            padding: 10px 12px;
            border-bottom: 1px solid #e1e4e8;
            font-size: 14px;
        }}
        @media (min-width: 768px) {{
            td {{
                padding: 12px 20px;
                font-size: 16px;
            }}
        }}
        tr:last-child td {{
            border-bottom: none;
        }}
        tr:hover {{
            background: #f6f8fa;
        }}
        .status {{
            display: inline-block;
            padding: 4px 8px;
            border-radius: 3px;
            font-size: 12px;
            font-weight: 600;
        }}
        .status.passed {{
            background: #dcffe4;
            color: #28a745;
        }}
        .status.failed {{
            background: #ffdce0;
            color: #d73a49;
        }}
        .failure-message {{
            color: #d73a49;
            font-size: 12px;
            margin-top: 5px;
            font-family: 'SFMono-Regular', Consolas, 'Liberation Mono', Menlo, monospace;
        }}
        .footer {{
            margin-top: 30px;
            padding-top: 15px;
            border-top: 1px solid #e1e4e8;
            text-align: center;
            color: #586069;
            font-size: 12px;
        }}
        @media (min-width: 768px) {{
            .footer {{
                margin-top: 40px;
                padding-top: 20px;
                font-size: 14px;
            }}
        }}
        .github-link {{
            color: #0366d6;
            text-decoration: none;
        }}
        .github-link:hover {{
            text-decoration: underline;
        }}
        .toggle-icon {{
            transition: transform 0.2s;
        }}
        .toggle-icon.collapsed {{
            transform: rotate(-90deg);
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>W3C SCXML Test Results</h1>
        <div class="timestamp">Generated on {datetime.now().strftime('%Y-%m-%d %H:%M:%S UTC')}</div>

        <div class="summary">
            <div class="summary-card total">
                <h3>Total Tests</h3>
                <div class="value">{data['total_tests']}</div>
                <div class="pass-rate">{pass_rate:.1f}% pass rate</div>
            </div>
            <div class="summary-card passed">
                <h3>Passed</h3>
                <div class="value">{data['total_passed']}</div>
            </div>
            <div class="summary-card failed">
                <h3>Failed</h3>
                <div class="value">{data['total_failures'] + data['total_errors']}</div>
            </div>
        </div>

        <div class="summary">
"""

    # Calculate per-engine statistics
    for suite in data['testsuites']:
        suite_passed = suite['tests'] - suite['failures'] - suite['errors']
        suite_pass_rate = (suite_passed / suite['tests'] * 100) if suite['tests'] > 0 else 0
        engine_name = "Interpreter" if "Interpreter" in suite['name'] else "AOT"
        
        html += f"""
            <div class="summary-card">
                <h3>{engine_name} Engine</h3>
                <div class="value">{suite_passed}/{suite['tests']}</div>
                <div class="pass-rate">{suite_pass_rate:.1f}% pass rate</div>
            </div>
"""

    html += """
        </div>
"""

    # Generate testsuites
    for suite in data['testsuites']:
        suite_passed = suite['tests'] - suite['failures'] - suite['errors']
        html += f"""
        <div class="testsuite">
            <div class="testsuite-header" onclick="toggleSuite(this)">
                <div class="testsuite-title">
                    <span class="toggle-icon">▼</span> {suite['name']}
                </div>
                <div class="testsuite-stats">
                    <span class="stat-passed">{suite_passed} passed</span>
                    <span class="stat-failed">{suite['failures']} failed</span>
                    <span>{suite['time']:.2f}s</span>
                </div>
            </div>
            <div class="testsuite-content">
                <table>
                    <thead>
                        <tr>
                            <th>Test</th>
                            <th>Status</th>
                            <th>Time</th>
                        </tr>
                    </thead>
                    <tbody>
"""

        for testcase in suite['testcases']:
            status_class = testcase['status']
            status_text = '✅ Pass' if status_class == 'passed' else '❌ Fail'
            failure_html = ''
            if testcase['message']:
                failure_html = f'<div class="failure-message">{testcase["message"]}</div>'

            html += f"""
                        <tr>
                            <td>
                                {testcase['name']}
                                {failure_html}
                            </td>
                            <td><span class="status {status_class}">{status_text}</span></td>
                            <td>{testcase['time']:.3f}s</td>
                        </tr>
"""

        html += """
                    </tbody>
                </table>
            </div>
        </div>
"""

    html += f"""
        <div class="footer">
            <p>
                View source on <a href="https://github.com/newmassrael/reactive-state-machine" class="github-link">GitHub</a>
                | Total execution time: {data['total_time']:.2f}s
            </p>
        </div>
    </div>

    <script>
        function toggleSuite(header) {{
            const content = header.nextElementSibling;
            const icon = header.querySelector('.toggle-icon');

            content.classList.toggle('collapsed');
            icon.classList.toggle('collapsed');
        }}
    </script>
</body>
</html>
"""

    return html

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: generate-html-report.py <input.xml> <output.html>")
        sys.exit(1)

    xml_path = sys.argv[1]
    html_path = sys.argv[2]

    try:
        data = parse_junit_xml(xml_path)
        html = generate_html(data)

        with open(html_path, 'w', encoding='utf-8') as f:
            f.write(html)

        print(f"HTML report generated: {html_path}")
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
