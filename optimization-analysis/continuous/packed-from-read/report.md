# Optimization 09: packed functional from-read materialization

Date: 2026-07-15

## Change

The prototype adds checked cross-relation packed row union and constructs
`fr = rf^-1 ; co` by one row OR per functional rf edge. It avoids both inverse-rf
allocation and scalar per-target insertion.

## Results

- Correctness: 142/142 tests, 39 mutation rows / 5,441 oracle checks, and 864 broad
  pairs with 852 comparable matches, 12 mutually unsupported, zero mismatch.
- Formal data: 3,456 rows, 36 XML, 36 log archives, and 43--48 measured overlap.
- Correct cells: 1,554 before and 1,556 after; zero common-solved verdict or safe-count
  mismatch. Four PSO status transitions represent two timeout cells becoming solved
  across repetitions, not changed verdicts.
- Strict aggregate CPU: 0.99996 [0.99557, 1.00444].
- CPU by model: SC 0.98671 [0.97922, 0.99420], TSO 0.99609
  [0.99011, 1.00193], PSO 1.00590 [0.99503, 1.01740].
- Aggregate wall: 1.00070 [0.99632, 1.00497].
- Aggregate RSS: 0.99998 [0.99982, 1.00015].

## Decision

Reject and remove. Aggregate CPU and wall intervals cross 1.0, while PSO's point
estimate regresses by 0.59%, beyond the pre-registered 0.5% tolerance. SC's 1.33%
benefit was discovered after observing the model split and cannot justify post-hoc
SC-only enablement. Production and test sources were restored exactly to HEAD; no
further fr micro-specialization is planned.
