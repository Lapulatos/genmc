# Statistical appendix

Wall times are strongly skewed and resource-censored, so no normality-based test is used. Each task contributes one median per backend. Geometric mean ratios use a deterministic 10,000-sample task bootstrap; the exact paired sign test discards exact ties and Holm-corrects the planned baseline contrasts. Solved-only ratios cannot describe coverage, so timeout/OOM counts and the failure-aware performance profile are co-primary evidence.
