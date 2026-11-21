# Local measurement tool

Configure with `-DSTRATA_BUILD_BENCHMARKS=ON`, build, then run `QT_QPA_PLATFORM=offscreen ./build-release/strata-measure`. It generates temporary 1/10/100 MiB ASCII files with 80-column lines. It records decode/refusal time, Qt document population and first-layout time, and one insert plus event processing. It does not include syntax highlighting, rich rendering, real project workload, or user-perceived latency distributions. 10/100 MiB are refused, not edited.

Recorded data: docs/benchmarks.json. On this Debian x86_64 / Qt 6.11.2 machine: 1 MiB decode 8.45 ms, populate/layout 31.39 ms, insert/events 1.11 ms. These are one measurement each, not performance guarantees or speed comparisons.

`QT_QPA_PLATFORM=offscreen hyperfine --warmup 2 --runs 5 --export-json docs/startup-lifecycle.json 'build-release/strata-editor --smoke-test'` measured 591.2 ms mean (24.8 ms standard deviation) for the entire lifecycle, which includes an intentional 500 ms timer. This is **not startup latency**. Peak RSS about 54 MiB. Separate /usr/bin/time -v run measured 55,368 KiB peak RSS. Idle CPU, real typing/scrolling, X11/Wayland and comparative before/after measurements remain future work. No comparative speed claim is made.
