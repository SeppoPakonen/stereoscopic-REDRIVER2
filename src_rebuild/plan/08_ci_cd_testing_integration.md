# CI/CD Testing Integration Guide

## Overview

This document outlines how to integrate stereo regression testing into the continuous integration and continuous deployment (CI/CD) pipeline.

---

## 1. Testing Phases in CI/CD

### Build Stage
```
[Git Push/PR]
    ↓
[Build Release]
    ↓
[Build Test Artifacts]
```

### Test Stage
```
[Automated Tests]
├── Unit Tests (5 min)
├── Integration Tests (10 min)
├── Smoke Tests (5 min)
├── Performance Tests (15 min)
└── Regression Tests (20 min)
```

### Report Stage
```
[Generate Reports]
├── Test Results Summary
├── Performance Metrics
├── Regression Detection
└── Coverage Statistics
```

---

## 2. Automated Test Framework Integration

### Command-Line Test Execution

**Run All Tests**:
```bash
game.exe --test-mode=all --test-output=results/
```

**Run Regression Tests Only**:
```bash
game.exe --test-mode=regression --test-output=results/
```

**Run Performance Tests**:
```bash
game.exe --test-mode=performance --test-output=results/
```

**Run Specific Category**:
```bash
game.exe --test-mode=category:stereo_compat --test-output=results/
```

**Run with Baseline Comparison**:
```bash
game.exe --test-mode=all --test-baseline=baselines/baseline.csv --test-output=results/
```

### Environment Variables for Testing

```bash
# Enable test mode
REDRIVER2_TEST_MODE=1

# Set output directory
REDRIVER2_TEST_OUTPUT=/path/to/results/

# Enable debug logging
REDRIVER2_TEST_DEBUG=1

# Baseline file for regression comparison
REDRIVER2_TEST_BASELINE=/path/to/baseline.csv

# Test timeout (seconds)
REDRIVER2_TEST_TIMEOUT=3600

# Performance threshold
REDRIVER2_PERF_THRESHOLD=20
```

---

## 3. GitHub Actions Workflow Example

Create `.github/workflows/test.yml`:

```yaml
name: Stereo Regression Testing

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

jobs:
  build-and-test:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Setup Build Environment
      run: |
        cd src_rebuild
        mkdir -p build
    
    - name: Build Release
      run: |
        cd src_rebuild
        premake5 vs2019
        MSBuild build/REDRIVER2.sln /p:Configuration=Release /p:Platform=x64
    
    - name: Run Smoke Tests
      run: |
        $LASTEXITCODE = 0
        .\build\bin\Release\REDRIVER2.exe --test-mode=smoke --test-output=results/ || $true
        if (Test-Path results/smoke.log) {
          Get-Content results/smoke.log
        }
    
    - name: Run Regression Tests
      run: |
        .\build\bin\Release\REDRIVER2.exe --test-mode=regression `
          --test-baseline=plan/baselines/baseline.csv `
          --test-output=results/
    
    - name: Run Performance Tests
      run: |
        .\build\bin\Release\REDRIVER2.exe --test-mode=performance `
          --test-output=results/ `
          --test-perf-threshold=20
    
    - name: Generate Report
      if: always()
      run: |
        python3 scripts/generate_test_report.py results/ ${{ github.run_number }}
    
    - name: Upload Test Results
      if: always()
      uses: actions/upload-artifact@v2
      with:
        name: test-results-${{ matrix.os }}
        path: results/
    
    - name: Comment on PR
      if: github.event_name == 'pull_request' && always()
      uses: actions/github-script@v6
      with:
        script: |
          const fs = require('fs');
          const report = fs.readFileSync('results/summary.md', 'utf8');
          github.rest.issues.createComment({
            issue_number: context.issue.number,
            owner: context.repo.owner,
            repo: context.repo.repo,
            body: report
          })
    
    - name: Fail if Regressions
      run: |
        if (Test-Path results/regressions.txt) {
          $regressions = Get-Content results/regressions.txt | Measure-Object -Line
          if ($regressions.Lines -gt 0) {
            Write-Error "Regressions detected!"
            exit 1
          }
        }
```

---

## 4. Test Report Parsing and Analysis

### Report Format Specification

**JSON Report Format** (`results/report.json`):
```json
{
  "metadata": {
    "timestamp": "2024-01-15T10:30:00Z",
    "build_hash": "a1b2c3d4e5f6",
    "test_duration_ms": 45000
  },
  "summary": {
    "total": 150,
    "passed": 145,
    "failed": 5,
    "blocked": 0,
    "skipped": 0,
    "regressions": 2
  },
  "results": [
    {
      "test_id": 1,
      "test_name": "NonStereo_MissionUndercover",
      "category": "regression",
      "result": "PASS",
      "duration_ms": 120000,
      "is_regression": false
    }
  ],
  "regressions": [
    {
      "test_id": 42,
      "test_name": "Mode_Interlaced",
      "previous_result": "PASS",
      "current_result": "FAIL",
      "severity": "HIGH"
    }
  ],
  "performance": {
    "mode_anaglyph_simple_overhead_percent": 15.2,
    "mode_sidebyside_overhead_percent": 18.7,
    "all_modes_under_threshold": true
  }
}
```

### Test Report Python Script

Create `scripts/generate_test_report.py`:

```python
#!/usr/bin/env python3

import json
import sys
import os
from datetime import datetime

def parse_test_results(results_dir):
    """Parse test results from game output"""
    report_file = os.path.join(results_dir, "report.json")
    
    if not os.path.exists(report_file):
        print(f"ERROR: Report file not found: {report_file}")
        return None
    
    with open(report_file, 'r') as f:
        return json.load(f)

def generate_markdown_report(report, build_number):
    """Generate markdown summary for PR comments"""
    summary = report['summary']
    pass_rate = (summary['passed'] / summary['total'] * 100) if summary['total'] > 0 else 0
    
    md = f"""
## Test Results (Build #{build_number})

### Summary
- **Total Tests**: {summary['total']}
- **Passed**: {summary['passed']} ✓
- **Failed**: {summary['failed']} ✗
- **Regressions**: {summary['regressions']} ⚠️
- **Pass Rate**: {pass_rate:.1f}%

### Status
"""
    
    if summary['failed'] == 0 and summary['regressions'] == 0:
        md += "🟢 **All tests passed!**\n"
    elif summary['regressions'] > 0:
        md += f"🔴 **{summary['regressions']} regression(s) detected**\n"
        md += "\n### Regressions\n"
        for reg in report.get('regressions', []):
            md += f"- **{reg['test_name']}**: {reg['previous_result']} → {reg['current_result']}\n"
    else:
        md += f"🟡 **{summary['failed']} test(s) failed**\n"
    
    # Performance summary
    perf = report.get('performance', {})
    if perf:
        md += "\n### Performance\n"
        if perf.get('all_modes_under_threshold'):
            md += "✓ All stereo modes within performance threshold (<20% overhead)\n"
        else:
            md += "⚠️ Some stereo modes exceed performance threshold\n"
    
    return md

def main():
    if len(sys.argv) < 2:
        print("Usage: generate_test_report.py <results_dir> [build_number]")
        sys.exit(1)
    
    results_dir = sys.argv[1]
    build_number = sys.argv[2] if len(sys.argv) > 2 else "unknown"
    
    report = parse_test_results(results_dir)
    if report is None:
        sys.exit(1)
    
    # Generate markdown report
    markdown = generate_markdown_report(report, build_number)
    summary_file = os.path.join(results_dir, "summary.md")
    with open(summary_file, 'w') as f:
        f.write(markdown)
    
    print("Report generated:", summary_file)
    print("\n" + markdown)
    
    # Exit with error if tests failed
    if report['summary']['failed'] > 0 or report['summary']['regressions'] > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()
```

---

## 5. Performance Regression Detection

### Performance Threshold Configuration

Create `plan/perf_config.json`:

```json
{
  "thresholds": {
    "stereo_overhead_max_percent": 20,
    "frame_time_variance_max_percent": 10,
    "memory_leak_max_mb_per_30min": 5,
    "frame_rate_min_fps": 55
  },
  "target_scenarios": {
    "menu_idle": {
      "target_fps": 60,
      "max_variance": 2
    },
    "simple_mission": {
      "target_fps": 60,
      "max_variance": 10
    },
    "dense_traffic": {
      "target_fps": 60,
      "max_variance": 15
    }
  }
}
```

### Performance Test Script

Create `scripts/check_performance.py`:

```python
#!/usr/bin/env python3

import json
import sys

def check_performance(results_file, config_file):
    """Check if performance meets thresholds"""
    
    with open(results_file) as f:
        results = json.load(f)
    
    with open(config_file) as f:
        config = json.load(f)
    
    thresholds = config['thresholds']
    perf_data = results.get('performance', {})
    
    failures = []
    
    # Check stereo overhead
    for mode in ['anaglyph_simple', 'sidebyside', 'topbottom', 'interlaced']:
        key = f"mode_{mode}_overhead_percent"
        if key in perf_data:
            overhead = perf_data[key]
            if overhead > thresholds['stereo_overhead_max_percent']:
                failures.append(
                    f"{mode}: overhead {overhead:.1f}% > {thresholds['stereo_overhead_max_percent']}%"
                )
    
    # Check frame rate
    if 'min_frame_rate' in perf_data:
        min_fps = perf_data['min_frame_rate']
        if min_fps < thresholds['frame_rate_min_fps']:
            failures.append(
                f"Frame rate: {min_fps} FPS < {thresholds['frame_rate_min_fps']} FPS"
            )
    
    # Check memory leaks
    if 'memory_leak_mb_30min' in perf_data:
        leak = perf_data['memory_leak_mb_30min']
        if leak > thresholds['memory_leak_max_mb_per_30min']:
            failures.append(
                f"Memory leak: {leak} MB > {thresholds['memory_leak_max_mb_per_30min']} MB per 30 min"
            )
    
    if failures:
        print("Performance threshold violations:")
        for failure in failures:
            print(f"  ✗ {failure}")
        return 1
    else:
        print("✓ All performance thresholds met")
        return 0

if __name__ == "__main__":
    sys.exit(check_performance("results/report.json", "plan/perf_config.json"))
```

---

## 6. Baseline Management

### Establishing a New Baseline

```bash
# Build release version
cd src_rebuild
premake5 vs2019
MSBuild build/REDRIVER2.sln /p:Configuration=Release

# Run tests without baseline (generates new baseline)
.\build\bin\Release\REDRIVER2.exe --test-mode=all --test-output=baseline_run/

# Save as new baseline
cp baseline_run/report.json plan/baselines/baseline_v1.0.json
cp baseline_run/report.csv plan/baselines/baseline_v1.0.csv
```

### Baseline Versioning

Maintain baseline files with version numbers:

```
plan/baselines/
├── baseline_v0.9.csv
├── baseline_v1.0.csv  (current)
└── baseline_v1.0.json
```

### Baseline Update Process

1. After major feature addition/optimization:
   - Run full test suite
   - Review performance impact
   - If acceptable, save new baseline
   - Commit baseline file with commit message

Example commit:
```
Baseline: Update stereo performance baseline after optimization
- Baseline v1.1 includes interlaced mode optimization
- Stereo overhead reduced from 18% to 15%
```

---

## 7. Automated Nightly Testing

### Scheduled Test Run Configuration

Create `.github/workflows/nightly-tests.yml`:

```yaml
name: Nightly Regression Testing

on:
  schedule:
    - cron: '0 2 * * *'  # Run at 2 AM UTC daily

jobs:
  nightly-test:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Build Latest
      run: |
        cd src_rebuild
        premake5 vs2019
        MSBuild build/REDRIVER2.sln /p:Configuration=Release /p:Platform=x64
    
    - name: Run Full Test Suite
      run: |
        $env:REDRIVER2_TEST_MODE = 'all'
        $env:REDRIVER2_TEST_BASELINE = 'plan/baselines/baseline_current.csv'
        .\build\bin\Release\REDRIVER2.exe --test-mode=all `
          --test-baseline=plan/baselines/baseline_current.csv `
          --test-output=nightly_results/
    
    - name: Analyze Results
      run: |
        python3 scripts/analyze_nightly_results.py nightly_results/
    
    - name: Generate Report
      run: |
        python3 scripts/generate_nightly_report.py nightly_results/ ${{ secrets.GITHUB_TOKEN }}
    
    - name: Create Issue if Regression
      if: failure()
      uses: actions/create-issue@v1
      with:
        title: "Nightly Test: Regression Detected"
        body: |
          Regressions detected in nightly test run.
          
          See artifacts for detailed results.
          
          Run: ${{ github.server_url }}/${{ github.repository }}/actions/runs/${{ github.run_id }}
```

---

## 8. Local Testing Commands

### Quick Local Test Run

For developers to test locally before committing:

```bash
# Windows
set REDRIVER2_TEST_MODE=smoke
set REDRIVER2_TEST_OUTPUT=local_results/
game.exe

# Linux
export REDRIVER2_TEST_MODE=smoke
export REDRIVER2_TEST_OUTPUT=local_results/
./game
```

### Pre-Commit Hook

Create `.git/hooks/pre-commit` (or `scripts/pre-commit.sh`):

```bash
#!/bin/bash

# Run smoke tests before commit
echo "Running smoke tests..."
./game.exe --test-mode=smoke --test-output=pre_commit_results/

if [ $? -ne 0 ]; then
    echo "ERROR: Smoke tests failed. Commit aborted."
    echo "Fix issues and try again, or use --no-verify to skip."
    exit 1
fi

echo "Smoke tests passed!"
exit 0
```

---

## 9. Test Result Dashboarding

### Create Dashboard Script

Create `scripts/dashboard.py`:

```python
#!/usr/bin/env python3

import json
import glob
import os
from datetime import datetime

def create_dashboard(results_dir):
    """Create HTML dashboard of test results"""
    
    # Collect results from all test runs
    results_files = glob.glob(os.path.join(results_dir, "*/report.json"))
    results_files.sort()
    
    html = """
    <html>
    <head>
        <title>REDRIVER2 Stereo Test Dashboard</title>
        <style>
            body { font-family: Arial; margin: 20px; }
            table { border-collapse: collapse; width: 100%; }
            th, td { border: 1px solid #ccc; padding: 8px; text-align: left; }
            th { background-color: #4CAF50; color: white; }
            .pass { background-color: #90EE90; }
            .fail { background-color: #FFB6C6; }
            .regression { background-color: #FFD700; }
            .chart { width: 100%; height: 300px; margin: 20px 0; }
        </style>
    </head>
    <body>
        <h1>REDRIVER2 Stereo Regression Test Dashboard</h1>
        <p>Last updated: {timestamp}</p>
        
        <h2>Recent Test Runs</h2>
        <table>
            <tr>
                <th>Build</th>
                <th>Timestamp</th>
                <th>Pass Rate</th>
                <th>Regressions</th>
                <th>Status</th>
            </tr>
    """.format(timestamp=datetime.now().isoformat())
    
    for results_file in results_files[-10:]:  # Last 10 runs
        with open(results_file) as f:
            data = json.load(f)
        
        summary = data['summary']
        pass_rate = (summary['passed'] / summary['total'] * 100) if summary['total'] > 0 else 0
        
        status_class = "pass" if summary['regressions'] == 0 and summary['failed'] == 0 else "fail"
        status_text = "✓ PASS" if status_class == "pass" else "✗ FAIL"
        
        html += f"""
            <tr class="{status_class}">
                <td>{data['metadata']['build_hash']}</td>
                <td>{data['metadata']['timestamp']}</td>
                <td>{pass_rate:.1f}%</td>
                <td>{summary['regressions']}</td>
                <td>{status_text}</td>
            </tr>
        """
    
    html += """
        </table>
    </body>
    </html>
    """
    
    dashboard_file = os.path.join(results_dir, "dashboard.html")
    with open(dashboard_file, 'w') as f:
        f.write(html)
    
    print(f"Dashboard generated: {dashboard_file}")

if __name__ == "__main__":
    create_dashboard("results/")
```

---

## 10. Test Failure Notifications

### Slack Integration

Create `scripts/notify_slack.py`:

```python
#!/usr/bin/env python3

import json
import requests
import sys

def notify_slack(results_file, webhook_url):
    """Send test results to Slack"""
    
    with open(results_file) as f:
        results = json.load(f)
    
    summary = results['summary']
    regressions = results.get('regressions', [])
    
    # Determine emoji and color
    if regressions:
        emoji = "🔴"
        color = "danger"
        status = "REGRESSIONS DETECTED"
    elif summary['failed'] > 0:
        emoji = "🟡"
        color = "warning"
        status = "TESTS FAILED"
    else:
        emoji = "🟢"
        color = "good"
        status = "ALL TESTS PASSED"
    
    # Build message
    message = {
        "attachments": [
            {
                "color": color,
                "title": f"{emoji} Stereo Test Results",
                "fields": [
                    {"title": "Status", "value": status, "short": True},
                    {"title": "Total", "value": str(summary['total']), "short": True},
                    {"title": "Passed", "value": str(summary['passed']), "short": True},
                    {"title": "Failed", "value": str(summary['failed']), "short": True},
                    {"title": "Regressions", "value": str(summary['regressions']), "short": True},
                ]
            }
        ]
    }
    
    # Add regression details
    if regressions:
        regression_text = "\n".join([
            f"• {r['test_name']}: {r['previous_result']} → {r['current_result']}"
            for r in regressions[:5]
        ])
        message["attachments"][0]["fields"].append({
            "title": "Regressions",
            "value": regression_text,
            "short": False
        })
    
    # Send to Slack
    response = requests.post(webhook_url, json=message)
    return response.status_code == 200

if __name__ == "__main__":
    webhook = sys.argv[1] if len(sys.argv) > 1 else None
    if webhook:
        success = notify_slack("results/report.json", webhook)
        sys.exit(0 if success else 1)
```

---

## 11. Test Execution Best Practices

### Test Isolation
- Each test should be independent
- Clean state before each test
- No test should affect other tests

### Performance Testing
- Warm up system before measuring
- Run multiple times and average
- Account for system variance
- Use consistent hardware configuration

### Regression Detection
- Always maintain baseline
- Compare against previous stable version
- Flag performance regressions early
- Document known issues separately

### Artifact Management
- Keep test outputs for analysis
- Archive results by build/date
- Implement cleanup policies (e.g., keep 30 days)
- Use S3 or artifact server for long-term storage

---

## 12. Troubleshooting CI/CD Test Issues

### Common Issues and Solutions

**Issue**: Tests timeout
- **Solution**: Increase timeout, profile slow tests, optimize code

**Issue**: Flaky tests (intermittent failures)
- **Solution**: Add retry logic, check for timing issues, improve sync

**Issue**: Performance varies between runs
- **Solution**: Warm up before measuring, minimize background load, average multiple runs

**Issue**: Regressions not detected
- **Solution**: Update baseline after legitimate improvements, improve detection sensitivity

**Issue**: Tests fail on certain hardware
- **Solution**: Add hardware-specific handling, skip unsupported tests, document hardware requirements

---

