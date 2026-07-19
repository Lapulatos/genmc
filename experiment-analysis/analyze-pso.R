args <- commandArgs(trailingOnly = TRUE)
input <- if (length(args) >= 1) args[[1]] else "experiment-analysis/pso-raw-results.tsv"
outdir <- if (length(args) >= 2) args[[2]] else "experiment-analysis/analysis-output"
d <- read.delim(input, stringsAsFactors = FALSE)
stopifnot(nrow(d) == 100L, setequal(unique(d$backend), c("cat", "caat")), unique(d$model) == "pso")
contract <- aggregate(cbind(status, complete_executions, blocked_executions) ~ program + repetition,
                      d, function(x) length(unique(x)))
stopifnot(all(contract$status == 1 & contract$complete_executions == 1 & contract$blocked_executions == 1))

med <- aggregate(cbind(real_seconds, peak_rss_bytes) ~ backend + program, d, median)
w <- reshape(med, idvar = "program", timevar = "backend", direction = "wide")
w$time_ratio_caat_vs_cat <- w$real_seconds.caat / w$real_seconds.cat
w$rss_ratio_caat_vs_cat <- w$peak_rss_bytes.caat / w$peak_rss_bytes.cat
write.table(w, file.path(outdir, "pso-cell-summary.tsv"), sep = "\t", row.names = FALSE, quote = FALSE)

tot <- aggregate(real_seconds ~ backend + repetition, d, sum)
tw <- reshape(tot, idvar = "repetition", timevar = "backend", direction = "wide")
ratio <- tw$real_seconds.caat / tw$real_seconds.cat
delta <- tw$real_seconds.caat - tw$real_seconds.cat
n <- length(ratio); m <- mean(ratio); e <- qt(.975, n - 1) * sd(ratio) / sqrt(n)
test <- wilcox.test(delta, mu = 0, exact = FALSE)
summary <- data.frame(
  repetitions = n, cat_total_seconds_mean = mean(tw$real_seconds.cat),
  caat_total_seconds_mean = mean(tw$real_seconds.caat), ratio_mean = m,
  ratio_sd = sd(ratio), ratio_ci_low = m - e, ratio_ci_high = m + e,
  median_delta_seconds = median(delta), p_value = test$p.value,
  rss_ratio_geomean = exp(mean(log(w$rss_ratio_caat_vs_cat)))
)
write.table(summary, file.path(outdir, "pso-suite-summary.tsv"), sep = "\t", row.names = FALSE, quote = FALSE)
