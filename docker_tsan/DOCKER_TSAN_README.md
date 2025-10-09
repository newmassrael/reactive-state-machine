# Docker ThreadSanitizer Environment

This directory contains a lightweight Docker environment for ThreadSanitizer testing with glibc DNS resolver crash workarounds.

## Why This Environment?

ThreadSanitizer (TSAN) is essential for detecting race conditions, but running it with standard glibc can cause crashes due to nscd (Name Service Cache Daemon) thread safety issues. This Docker environment provides:

- **Automatic TSAN activation** via `IN_DOCKER_TSAN=1` environment variable
- **nscd crash workarounds** - prevents glibc DNS resolver TLS crashes
- **No suppressions needed** - clean race detection in your code
- **Isolated environment** - no impact on host system
- **Fast setup** - 5-10 minutes build time
- **Reproducible testing** - consistent TSAN behavior

## Key Features

**Local Development (Default)**
- TSAN: Disabled by default (ENABLE_TSAN=OFF)
- Use AddressSanitizer for fast memory error detection
- Manual TSAN enable: `cmake -DENABLE_TSAN=ON ..`

**Docker TSAN Environment**
- TSAN: Automatically enabled via `IN_DOCKER_TSAN=1`
- System glibc with nscd workarounds
- `ignore_noninstrumented_modules=1` for system libraries
- Complete race detection in your application code

## Prerequisites

- Docker installed and running
- ~1GB free disk space
- 5-10 minutes for initial build

## Quick Start

**Single command to do everything:**

```bash
cd docker_tsan
./docker-tsan-run.sh
```

**First run:**
- Automatically detects missing Docker image
- Prompts for confirmation to build (y/N)
- Builds Docker image (~5-10 minutes, one-time)
- Automatically builds project with TSAN
- Enters interactive shell in `/workspace/build`

**Subsequent runs:**
- Skips image build (already exists)
- Automatically builds project with TSAN
- Enters interactive shell in `/workspace/build`

**Inside the container:**

```bash
# Run full test suite
root@container:/workspace/build# ctest --output-on-failure

# Run specific test
root@container:/workspace/build# ./tests/w3c_test_cli <test_number>

# Exit container
root@container:/workspace/build# exit
```

## Expected Results

With nscd workarounds:
- ThreadSanitizer detects race conditions without crashes
- DNS resolver calls work correctly (no TLS corruption)
- All tests run with full thread safety validation
- **No suppressions needed** - clean race detection
- Automatic TSAN activation (no manual `-DENABLE_TSAN=ON` required)

## Technical Details

### How nscd Workarounds Work

**Problem:**
- glibc's `getaddrinfo()` uses nscd for DNS caching
- nscd has thread safety issues that trigger TSAN crashes
- Crashes occur in TLS (Thread-Local Storage) initialization

**Solution 1: Disable nscd**
```dockerfile
RUN systemctl disable nscd 2>/dev/null || true
```
Prevents nscd from running in the container.

**Solution 2: Configure nsswitch.conf**
```dockerfile
RUN echo "hosts: files dns" > /etc/nsswitch.conf
```
Forces direct DNS resolution bypassing nscd entirely.

**Solution 3: TSAN Options**
```dockerfile
ENV TSAN_OPTIONS="ignore_noninstrumented_modules=1"
```
Ignores memory accesses in non-instrumented system libraries.

## Troubleshooting

### Build fails with "Permission denied"

```bash
chmod +x docker-tsan-run.sh
```

### Out of disk space

The build requires ~1GB:
- Base Ubuntu image: ~200MB
- Build tools: ~400MB
- Dependencies: ~300MB
- Final image: ~1GB

### Build takes too long

Expected build time:
- 4-core system: 5-10 minutes
- 8-core system: 3-5 minutes
- 16-core system: 2-3 minutes

### TSAN still crashes

If you still see crashes:
1. Check nscd is not running: `ps aux | grep nscd`
2. Verify nsswitch.conf: `cat /etc/nsswitch.conf`
3. Check TSAN_OPTIONS: `echo $TSAN_OPTIONS`

## Architecture

```
┌─────────────────────────────────────┐
│  Docker Container (Ubuntu 22.04)   │
├─────────────────────────────────────┤
│  ENV IN_DOCKER_TSAN=1              │
│  ENV TSAN_OPTIONS="..."            │
│  (Auto-enables TSAN in CMake)      │
├─────────────────────────────────────┤
│  System glibc 2.35                 │
│  + nscd disabled                   │
│  + nsswitch.conf configured        │
├─────────────────────────────────────┤
│  Your Project (mounted volume)      │
│  (/workspace)                       │
└─────────────────────────────────────┘
```

**CMake Auto-Detection Flow:**
1. Dockerfile sets `ENV IN_DOCKER_TSAN=1`
2. CMakeLists.txt detects `$ENV{IN_DOCKER_TSAN}`
3. Automatically sets `ENABLE_TSAN=ON`
4. Builds with `-fsanitize=thread` flags
5. No manual configuration needed

## Performance

ThreadSanitizer adds significant overhead:
- CPU: 2-20x slower
- Memory: 5-10x more usage
- Disk I/O: 2-3x slower

This is normal and expected.

## Maintenance

### Rebuild image

```bash
docker rmi rsm-tsan-env:latest
./docker-tsan-run.sh  # Will prompt to rebuild
```

### Clean up

```bash
# Remove image
docker rmi rsm-tsan-env:latest

# Remove build logs
rm docker-tsan-build.log
```

## Comparison: Regular vs TSAN Environment

| Aspect | Regular Debug (ASAN) | TSAN Docker |
|--------|---------------------|-------------|
| Setup time | Instant | 5-10 minutes (first time) |
| TSAN activation | Manual (`-DENABLE_TSAN=ON`) | Automatic ✅ |
| Memory errors | Detected | Detected |
| Race detection | None | Full ✅ |
| Suppressions | N/A | None needed ✅ |
| DNS crashes | N/A | Prevented ✅ |
| Performance | 2x slower | 5-10x slower |
| Disk space | Minimal | ~1GB |

## When to Use

**Use TSAN Docker when:**
- Debugging race conditions or data races
- Verifying thread safety of async/parallel code
- Investigating intermittent test failures
- Working with multi-threaded event handling
- CI/CD pipeline for comprehensive testing

**Use regular Debug (AddressSanitizer) when:**
- Quick development iteration
- Memory error detection
- Non-threading bugs
- General-purpose debugging

## References

### Why nscd Causes TSAN Crashes

- [glibc Bug #16743](https://sourceware.org/bugzilla/show_bug.cgi?id=16743): getaddrinfo uses uninitialized data when processing nscd answer
- [Google Sanitizers Issue #1409](https://github.com/google/sanitizers/issues/1409): Detect dynamic TLS allocations for glibc>=2.19
- [Fedora: Remove nscd](https://lists.fedoraproject.org/archives/list/devel@lists.fedoraproject.org/thread/4K634Q3567QMMVJIGXM6I6MOJPOWO6QF/): Deprecate nscd in favor of sssd and systemd-resolved

### ThreadSanitizer Documentation

- [Clang ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html)
- [Google Sanitizers Wiki](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)

## Additional Notes

- The container uses `/workspace` as working directory
- Your project directory is mounted (changes persist on host)
- Container is removed after exit (no state accumulation)
- gdb and other debugging tools are pre-installed
- System glibc is used (not custom-built)
- nscd workarounds are applied automatically
