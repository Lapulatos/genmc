args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 2) stop("usage: plot_analysis.R <analysis-dir> <figures-dir>")
analysis_dir <- args[[1]]
figures_dir <- args[[2]]
dir.create(figures_dir, recursive = TRUE, showWarnings = FALSE)

summary <- read.delim(file.path(analysis_dir, "metric-summary.tsv"), stringsAsFactors = FALSE)
ratios <- read.delim(file.path(analysis_dir, "ratios.tsv"), stringsAsFactors = FALSE)
profile <- read.delim(file.path(analysis_dir, "profile-pairs.tsv"), stringsAsFactors = FALSE)
groups <- c("all", "sc", "tso", "pso")
labels <- c("All (task-clustered)", "SC", "TSO", "PSO")
colors <- c("#000000", "#0072B2", "#E69F00", "#CC79A7")

draw_metric <- function() {
  par(mfrow = c(1, 3), mar = c(4.5, 7.5, 2.0, 1.0), oma = c(0, 0, 0, 0))
  for (metric in c("cpu", "wall", "memory")) {
    data <- summary[summary$metric == metric, ]
    data <- data[match(groups, data$group), ]
    y <- rev(seq_along(groups))
    xlim <- if (metric == "memory") c(0.9993, 1.0005) else c(0.94, 1.04)
    plot(data$geomean_ratio, y, xlim = xlim, ylim = c(0.5, 4.5), yaxt = "n",
         ylab = "", xlab = "After / before", pch = 19, col = colors,
         main = toupper(metric), bty = "l")
    axis(2, at = y, labels = labels, las = 1, cex.axis = 0.8)
    abline(v = 1, lty = 2, col = "#777777")
    segments(data$ci_low, y, data$ci_high, y, col = colors, lwd = 2)
    points(data$geomean_ratio, y, pch = 19, col = colors)
  }
}

pdf(file.path(figures_dir, "figure-01-metric-ratios.pdf"), width = 11, height = 4.2, useDingbats = FALSE)
draw_metric()
dev.off()
svg(file.path(figures_dir, "figure-01-metric-ratios.svg"), width = 11, height = 4.2)
draw_metric()
dev.off()

draw_cpu_distribution <- function() {
  data <- ratios[ratios$metric == "cpu", ]
  data$group <- factor(data$group, levels = groups, labels = labels)
  boxplot(ratio ~ group, data = data, log = "y", col = colors, border = "#333333",
          ylab = "CPU time ratio (after / before, log scale)", xlab = "",
          outline = FALSE, las = 1)
  abline(h = 1, lty = 2, col = "#555555")
  set.seed(20260715)
  for (index in seq_along(levels(data$group))) {
    values <- data$ratio[data$group == levels(data$group)[index]]
    points(jitter(rep(index, length(values)), amount = 0.10), values,
           pch = 16, cex = 0.35, col = adjustcolor(colors[index], alpha.f = 0.45))
  }
}

pdf(file.path(figures_dir, "figure-02-cpu-task-distribution.pdf"), width = 7.2, height = 4.8, useDingbats = FALSE)
draw_cpu_distribution()
dev.off()
svg(file.path(figures_dir, "figure-02-cpu-task-distribution.svg"), width = 7.2, height = 4.8)
draw_cpu_distribution()
dev.off()

draw_history <- function() {
  before <- profile$before_max_history_base_bytes
  after <- profile$after_max_history_base_bytes
  limit <- max(log10(1 + c(before, after)))
  plot(log10(1 + before), log10(1 + after), xlim = c(0, limit), ylim = c(0, limit),
       xlab = "Before max history bytes (log10(1+x))",
       ylab = "After max history bytes (log10(1+x))", pch = 16,
       col = adjustcolor("#0072B2", alpha.f = 0.45), asp = 1)
  abline(0, 1, lty = 2, col = "#555555")
}

pdf(file.path(figures_dir, "figure-03-history-memory.pdf"), width = 5.4, height = 5.2, useDingbats = FALSE)
draw_history()
dev.off()
svg(file.path(figures_dir, "figure-03-history-memory.svg"), width = 5.4, height = 5.2)
draw_history()
dev.off()
