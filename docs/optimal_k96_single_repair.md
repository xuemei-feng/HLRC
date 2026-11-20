# OptimalLRC k=96 single-block repair

For `OptimalLRC` with `k=96`, `r=3`, and `z=4`, the registered single-block benchmark contains exactly one case: `L0`. This restriction applies only to benchmark case registration; the underlying OptimalLRC recovery implementation remains generic and can still recover other supported blocks when called directly.

## Registered case

- Case: `L0`
- Failed block ID: `99`
- Source block IDs, in order: `0..23, 96, 97, 98` (`D0..D23, G0..G2`)

No `G0` single-block case is registered for this configuration. The existing `k=128` uneven single cases and the dedicated `k=64` single cases are unchanged.

## Build and run

```bash
cmake -S project -B project/build
cmake --build project/build --target repair_benchmark optimal_k96_single_repair_verify optimal_uneven_verify
./project/build/optimal_k96_single_repair_verify
./project/build/optimal_uneven_verify
python3 project/tests/repair_benchmark_dispatch_verify.py project/build/repair_benchmark
```

To inspect the exact benchmark plan without contacting a cluster, run:

```bash
./project/build/repair_benchmark 96 single --check-plan project/config/parameterConfiguration.xml
```

The configuration file used by that command must specify `OptimalLRC`, `k=96`, `r=3`, and `z=4`.
