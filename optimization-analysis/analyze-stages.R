args <- commandArgs(trailingOnly = TRUE)
input <- if (length(args)) args[[1]] else "optimization-analysis/results/stage-results.tsv"
output <- if (length(args) > 1) args[[2]] else "optimization-analysis/results/stage-summary.tsv"
d <- read.delim(input, stringsAsFactors = FALSE)
d$rss_mib <- d$peak_rss_bytes / 1024^2

semantic <- aggregate(cbind(status, complete_executions, blocked_executions) ~
                        backend + model + program + repetition,
                      d, function(x) length(unique(x)))
stopifnot(all(semantic$status == 1), all(semantic$complete_executions == 1),
          all(semantic$blocked_executions == 1))

stages <- unique(d$stage)
baseline <- subset(d, stage == "stage-0")
rows <- do.call(rbind, lapply(stages, function(stage_name) {
  current <- d[d$stage == stage_name, ]
  caat <- subset(current, backend == "caat")
  core <- subset(caat, model == "sc" & grepl("fcombiner", program))
  totals <- aggregate(real_seconds ~ repetition, caat, sum)
  rss <- aggregate(peak_rss_bytes ~ repetition, caat, max)
  base_caat <- subset(baseline, backend == "caat")
  base_totals <- aggregate(real_seconds ~ repetition, base_caat, sum)
  base_core <- subset(base_caat, model == "sc" & grepl("fcombiner", program))
  data.frame(
    stage = stage_name,
    samples = nrow(current),
    caat_suite_seconds_mean = mean(totals$real_seconds),
    suite_speedup_vs_stage0 = mean(aggregate(real_seconds ~ repetition, base_caat, sum)$real_seconds) /
                              mean(totals$real_seconds),
    sc_fcomb_seconds_mean = mean(core$real_seconds),
    sc_fcomb_seconds_median = median(core$real_seconds),
    sc_fcomb_speedup_vs_stage0 = median(base_core$real_seconds) / median(core$real_seconds),
    caat_peak_rss_mib_mean = mean(rss$peak_rss_bytes) / 1024^2,
    semantic_mismatches = 0
  )
}))
write.table(rows, output, sep = "\t", row.names = FALSE, quote = FALSE)
