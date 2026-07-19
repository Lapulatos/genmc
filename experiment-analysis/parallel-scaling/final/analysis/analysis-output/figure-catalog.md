# Figure catalog

## figure-01-completion.svg
Purpose: show coverage loss as worker count rises. Reader should notice that unknown parallel failures reduce solved task-runs. This changes the decision from "use more workers" to "stabilize the parallel runtime first."

## figure-02-wall-speedup.svg
Purpose: compare common-solved wall-time speedup against the 1.0 baseline. Reader should notice that nearly all points are close to or below 1.0. This blocks claims of scalable speedup on this corpus.

## figure-03-cpu-efficiency.svg
Purpose: expose aggregate CPU cost hidden by wall time. Reader should notice the monotonic drop toward roughly 0.73--0.77 at eight workers. This favors one worker for throughput.

## figure-04-rss.svg
Purpose: compare peak RSS on common-solved pairs. Reader should notice CAT/CAAT are near 1.0 while native GenMC loses more RSS efficiency. Ratios summarize paired peaks and do not represent simultaneous server-wide memory.
