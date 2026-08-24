# Outstanding Sequence-Diagram Corrections

The following three issues remain in `SEQUENCE_DIAGRAMS.md` and must be corrected before Section 4.2 is finalised.

## 1. SD-07 must acknowledge a safely deferred request promptly

SD-07 currently retains an override request as “unacknowledged” until the active pedestrian clearance completes. This conflicts with the requirement that every Central command receive a timely `ACK` or `NACK`.

If the request is safe but must wait for pedestrian clearance, `Lx` should immediately return:

```text
ACK(accepted, pending clearance)
```

After clearance completes, `Lx` must revalidate the retained request before applying it. It should report the resulting status rather than sending the initial acknowledgement for a second time. An unsafe request or one that cannot be safely deferred must still receive `NACK`.

## 2. SD-07 must validate and acknowledge renewal requests

The current renewal branch restarts the override timer without showing local validation or an `ACK/NACK` response. A renewal is another Central request and must not extend the override automatically.

For every `RENEW_OVERRIDE` request, `Lx` must revalidate:

- the requested duration and maximum permitted limit;
- the current pedestrian-clearance state;
- the adjacent railway status;
- the local fault state and conflict matrix.

A valid renewal receives `ACK` and restarts the timer. An invalid or unsafe renewal receives `NACK`; the existing expiry time remains unchanged. Railway pre-emption or a local fault must still terminate the active override according to the safety-priority rules.

## 3. SD-05 must describe the queue detector correctly

The current note incorrectly states that the binary queue detector feeds railway pre-emption suppression. Suppression is triggered by the crossing state being other than `OPEN` (`CC-02`), not by `QUEUE_WARNING`.

The note should instead explain that:

- the detector reports only a binary `QUEUE_WARNING` (`CC-01`);
- it does not count vehicles or initiate railway suppression;
- its status is checked after the crossing reopens to control the bounded drain extension (`CC-03`).

Suggested replacement:

```text
The detector reports only a binary QUEUE_WARNING. Railway status triggers suppression; after reopening, QUEUE_WARNING controls the bounded drain extension (CC-01–CC-03).
```
