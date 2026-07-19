args <- commandArgs(trailingOnly = TRUE)
input <- if (length(args) >= 1) args[[1]] else "experiment-analysis/raw-results.tsv"
outdir <- if (length(args) >= 2) args[[2]] else "experiment-analysis/analysis-output"
dir.create(outdir, recursive = TRUE, showWarnings = FALSE)
dir.create(file.path(outdir, "figures"), recursive = TRUE, showWarnings = FALSE)

d <- read.delim(input, check.names = FALSE, stringsAsFactors = FALSE)
stopifnot(nrow(d) == 300L, !anyDuplicated(d[c("backend", "model", "program", "repetition")]))
d$peak_rss_mib <- d$peak_rss_bytes / 1024^2
d$cell <- paste(d$model, basename(dirname(dirname(d$program))), basename(d$program), sep = "/")

contract <- aggregate(cbind(status, complete_executions, blocked_executions) ~ model + program + repetition,
                      d, function(x) length(unique(x)))
contract_ok <- all(contract$status == 1 & contract$complete_executions == 1 & contract$blocked_executions == 1)
if (!contract_ok) stop("semantic comparison contract failed")

mean_ci <- function(x) {
  n <- length(x); m <- mean(x); s <- sd(x); e <- qt(.975, n - 1) * s / sqrt(n)
  c(n = n, mean = m, sd = s, median = median(x), ci_low = m - e, ci_high = m + e,
    q25 = unname(quantile(x, .25)), q75 = unname(quantile(x, .75)))
}

cells <- unique(d[c("backend", "model", "program")])
desc <- do.call(rbind, lapply(seq_len(nrow(cells)), function(i) {
  z <- merge(cells[i, , drop = FALSE], d)
  cbind(cells[i, , drop = FALSE], as.data.frame(t(mean_ci(z$real_seconds))),
        rss_mean_mib = mean(z$peak_rss_mib), rss_sd_mib = sd(z$peak_rss_mib),
        executions = unique(z$complete_executions), blocked = unique(z$blocked_executions))
}))
write.table(desc, file.path(outdir, "cell-summary.tsv"), sep = "\t", row.names = FALSE, quote = FALSE)

med <- aggregate(cbind(real_seconds, peak_rss_mib) ~ backend + model + program, d, median)
base <- med[med$backend == "genmc", c("model", "program", "real_seconds", "peak_rss_mib")]
names(base)[3:4] <- c("base_time", "base_rss")
ratios <- merge(med[med$backend != "genmc", ], base, by = c("model", "program"))
ratios$time_ratio <- ratios$real_seconds / ratios$base_time
ratios$rss_ratio <- ratios$peak_rss_mib / ratios$base_rss
ratios$rss_delta_mib <- ratios$peak_rss_mib - ratios$base_rss
write.table(ratios, file.path(outdir, "normalized-cell-ratios.tsv"), sep = "\t", row.names = FALSE, quote = FALSE)

rank_biserial <- function(delta) {
  delta <- delta[delta != 0]
  ranks <- rank(abs(delta), ties.method = "average")
  (sum(ranks[delta > 0]) - sum(ranks[delta < 0])) / sum(ranks)
}
geo_ci <- function(x) {
  y <- log(x); n <- length(y); m <- mean(y); e <- qt(.975, n - 1) * sd(y) / sqrt(n)
  c(estimate = exp(m), ci_low = exp(m - e), ci_high = exp(m + e))
}

comparisons <- do.call(rbind, lapply(c("cat", "caat"), function(b) {
  z <- ratios[ratios$backend == b, ]
  wt <- wilcox.test(z$time_ratio, mu = 1, exact = FALSE, paired = FALSE)
  wr <- wilcox.test(z$rss_delta_mib, mu = 0, exact = FALSE, paired = FALSE)
  gt <- geo_ci(z$time_ratio); gr <- geo_ci(z$rss_ratio)
  data.frame(backend = b, cells = nrow(z), time_ratio = gt[1], time_ci_low = gt[2],
             time_ci_high = gt[3], time_p_raw = wt$p.value,
             time_rank_biserial = rank_biserial(z$time_ratio - 1),
             rss_ratio = gr[1], rss_ci_low = gr[2], rss_ci_high = gr[3],
             rss_delta_mib_median = median(z$rss_delta_mib), rss_p_raw = wr$p.value,
             rss_rank_biserial = rank_biserial(z$rss_delta_mib))
}))
comparisons$time_p_holm <- p.adjust(comparisons$time_p_raw, "holm")
comparisons$rss_p_holm <- p.adjust(comparisons$rss_p_raw, "holm")
write.table(comparisons, file.path(outdir, "pairwise-summary.tsv"), sep = "\t", row.names = FALSE, quote = FALSE)

totals <- aggregate(real_seconds ~ backend + repetition, d, sum)
totals_w <- reshape(totals, idvar = "repetition", timevar = "backend", direction = "wide")
names(totals_w) <- sub("real_seconds\\.", "", names(totals_w))
write.table(totals_w, file.path(outdir, "per-repetition-total-time.tsv"), sep = "\t", row.names = FALSE, quote = FALSE)

suite <- do.call(rbind, lapply(c("cat", "caat"), function(b) {
  ratio <- totals_w[[b]] / totals_w$genmc
  delta <- totals_w[[b]] - totals_w$genmc
  ci <- mean_ci(ratio)
  w <- wilcox.test(delta, mu = 0, paired = FALSE, exact = FALSE)
  data.frame(backend = b, repetitions = length(ratio), total_seconds_mean = mean(totals_w[[b]]),
             genmc_seconds_mean = mean(totals_w$genmc), ratio_mean = ci["mean"],
             ratio_sd = ci["sd"], ratio_ci_low = ci["ci_low"], ratio_ci_high = ci["ci_high"],
             delta_seconds_median = median(delta), p_raw = w$p.value,
             rank_biserial = rank_biserial(delta))
}))
suite$p_holm <- p.adjust(suite$p_raw, "holm")
write.table(suite, file.path(outdir, "suite-total-summary.tsv"), sep = "\t", row.names = FALSE, quote = FALSE)

cols <- c(genmc = "#0072B2", cat = "#E69F00", caat = "#D55E00")
make_main <- function(device, filename) {
  device(filename, width = 8.2, height = 4.8)
  old <- par(mar = c(8, 4.5, 1, 1)); on.exit({par(old); dev.off()}, add = TRUE)
  short_program <- function(x) {
    ifelse(grepl("fcombiner", x), "FComb",
    ifelse(grepl("ms-queue", x), "MSQ",
    ifelse(grepl("treiber", x), "Treiber",
    ifelse(grepl("RMWFix", x), "RMWFix", "SB"))))
  }
  labels <- paste(toupper(ratios$model), short_program(ratios$program), sep = "/")
  x <- seq_len(nrow(ratios)); y <- ratios$time_ratio
  plot(x, y, log = "y", xaxt = "n", xlab = "", ylab = "Median wall-time ratio vs built-in GenMC",
       pch = ifelse(ratios$backend == "cat", 16, 17), col = cols[ratios$backend], ylim = range(c(.8, y)))
  abline(h = 1, lty = 2, col = "grey40")
  axis(1, at = x, labels = labels, las = 2, cex.axis = .65)
  legend("topleft", c("GenMC+CAT", "GenMC+CAAT"), pch = c(16, 17), col = cols[c("cat", "caat")], bty = "n")
}
make_main(pdf, file.path(outdir, "figures", "figure-01-normalized-time.pdf"))
make_main(svg, file.path(outdir, "figures", "figure-01-normalized-time.svg"))

make_support <- function(device, filename) {
  device(filename, width = 7.2, height = 4.2)
  old <- par(mar = c(4, 4.5, 1, 1)); on.exit({par(old); dev.off()}, add = TRUE)
  f <- d[grepl("fcombiner", d$program), ]
  f$group <- factor(paste(toupper(f$model), f$backend, sep = "/"),
                    levels = as.vector(sapply(c("SC", "TSO"), function(m) paste(m, c("genmc", "cat", "caat"), sep = "/"))))
  boxplot(real_seconds ~ group, f, log = "y", ylab = "Wall time (s, log scale)", xlab = "",
          col = cols[sub(".*/", "", levels(f$group))], outline = TRUE)
}
make_support(pdf, file.path(outdir, "figures", "figure-02-fcombiner-distribution.pdf"))
make_support(svg, file.path(outdir, "figures", "figure-02-fcombiner-distribution.svg"))

make_memory <- function(device, filename) {
  device(filename, width = 7.2, height = 4.2)
  old <- par(mar = c(4, 4.5, 1, 1)); on.exit({par(old); dev.off()}, add = TRUE)
  boxplot(rss_delta_mib ~ backend, ratios, ylab = "Median peak-RSS delta vs GenMC (MiB)", xlab = "",
          names = c("GenMC+CAAT", "GenMC+CAT"), col = cols[c("caat", "cat")])
  abline(h = 0, lty = 2, col = "grey40")
}
make_memory(pdf, file.path(outdir, "figures", "figure-03-memory-overhead.pdf"))
make_memory(svg, file.path(outdir, "figures", "figure-03-memory-overhead.svg"))

writeLines(c(
  paste0("rows=", nrow(d)), paste0("semantic_contract_ok=", contract_ok),
  paste0("models=", paste(sort(unique(d$model)), collapse = ",")),
  paste0("programs=", length(unique(d$program))), paste0("repetitions=", length(unique(d$repetition)))
), file.path(outdir, "validation.txt"))
