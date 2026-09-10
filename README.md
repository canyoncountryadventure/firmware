# SEN0171 Meshtastic Trail-Counter Experiment

> **Branch:** `trail-sen0171`
>
> **Status:** Dedicated experimental/legacy trail-counter firmware. **Not HOBO production firmware.**
>
> **Sensor:** DFRobot SEN0171 PIR
>
> **Signal pin:** `D0`

This branch isolates the early SEN0171 trail-counter experiment from the production HOBO and CCA field-node firmware.

## What it does

When the dedicated trail-counter build flag is enabled, the node:

1. reads the SEN0171 digital output on `D0`;
2. waits for an initial HIGH to clear before arming;
3. counts each distinct LOW → HIGH transition as one PIR event;
4. broadcasts a Meshtastic text message such as:

```text
PERSON WALKED BY #12
```

For later events it may also include the measured LOW gap between PIR activations.

The module is compiled only when:

```text
TRAIL_COUNTER_SEN0171
```

is defined. This prevents the experimental trail code from being included accidentally in unrelated firmware targets.

## Important sensor limitation

This is **not a precise individual people counter**.

The SEN0171 can remain HIGH for several seconds. If multiple people pass while the sensor never returns LOW, the hardware presents one continuous event and firmware cannot reliably split that into separate people.

The branch is therefore useful for testing coarse visitation/activity detection, sensor placement, detection duration, and spacing between distinct PIR activations.

## Current event logic

- Poll interval: approximately 20 ms.
- Startup HIGH is ignored until the PIR first returns LOW.
- A LOW → HIGH transition increments the count once.
- The node does not count repeatedly while the PIR remains HIGH.
- A HIGH → LOW transition records the detection duration and rearms the next distinct event.

The current implementation uses a simple digital input on `D0`; it does **not** include the later RF-aware PIR filtering used by the CCA production PIR branches.

## Do not use this branch for CCA production PIR nodes

The production CCA PIR work is on:

```text
CCA-MX-HOBO-PIR-SEEED-v1
```

and the combined rock/soil variant is:

```text
CCA-MX-HOBO-PIR-ROCK-SEEED-v1
```

Those branches include the later PIR protections, including RF self-trigger rejection and private alert routing. Do not treat this trail experiment as a rollback source for that behavior.

## HOBO production firmware

The canonical HOBO field-node branch is:

```text
hobo-mx2001-mx2201-mx2203
```

That production line contains MX2001/MX2201/MX2203 support, pointer-gated automatic HOBO transmissions, and the `LOGGER`, `READ`, `LOCK`, and `UNLOCK` direct-message commands. This trail branch should not be flashed when those features are required.

## When to keep this branch

Keep `trail-sen0171` only while the dedicated trail-counter concept is still useful for development or field experiments. If that work is retired, preserve the final state as a tag and delete the branch rather than leaving it looking like a production firmware choice.
