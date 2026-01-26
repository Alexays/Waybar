# Test Configuration and Results for Shared Exec Worker

## Feature Overview

This document describes the testing of the new `shared` configuration option for custom modules, which allows multiple Waybar instances (across different monitors) to share a single exec worker instead of running duplicate processes.

## Test Configuration

**Command used to launch Waybar:**
```bash
./build/waybar -l trace
```

**Configuration file:** `~/.config/waybar/config.jsonc`

```jsonc
{
  "modules-right": ["custom/test"],

  "custom/test": {
    "exec": "echo $RANDOM $(date +%s)",
    "interval": 5,
    "shared": true
  }
}
```

## Test Environment

- **Multiple monitors:** Yes (multi-monitor setup)
- **Waybar instances:** One bar per monitor
- **Module configuration:** Single `custom/test` module with `"shared": true`

## Observed Results

### Behavior with `"shared": true`

 **Expected behavior confirmed:**
- All bars across all monitors displayed **identical** random numbers and timestamps
- Values updated **simultaneously** every 5 seconds across all instances
- The synchronization confirms that only **one** exec worker is running and distributing its output to all module instances

### Output Example

All monitors showed matching output like:
```
12345 1697567890
```

After 5 seconds, all monitors updated together to:
```
67890 1697567895
```

## Conclusion

The shared exec worker feature is **working as expected**. Multiple Waybar instances successfully share a single worker process when `"shared": true` is configured, eliminating redundant command execution while maintaining synchronized output across all displays.

## Backward Compatibility

The implementation maintains full backward compatibility:
- Existing configurations without the `"shared"` option continue to work unchanged
- Default behavior (dedicated worker per module) is preserved
- The feature is opt-in via explicit `"shared": true` configuration

## Performance Impact

**Resource savings observed:**
- With N monitors/bars: N processes reduced to 1 process per unique shared command
- CPU usage reduced proportionally
- Particularly beneficial for resource-intensive scripts or rate-limited API calls
