// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/engagement/site_engagement_score.h"

namespace {

// These numbers are used as suffixes for the
// SiteEngagementService.EngagementScoreBucket_* histogram. If these bases
// change, the EngagementScoreBuckets suffix in histograms.xml should be
// updated.
const int kEngagementBucketHistogramBuckets[] = {0,  10, 20, 30, 40, 50,
                                                 60, 70, 80, 90, 100};

}  // namespace

const char SiteEngagementMetrics::kTotalEngagementHistogram[] =
    "SiteEngagementService.TotalEngagement";

const char SiteEngagementMetrics::kTotalOriginsHistogram[] =
    "SiteEngagementService.OriginsEngaged";

const char SiteEngagementMetrics::kMeanEngagementHistogram[] =
    "SiteEngagementService.MeanEngagement";

const char SiteEngagementMetrics::kMedianEngagementHistogram[] =
    "SiteEngagementService.MedianEngagement";

const char SiteEngagementMetrics::kEngagementScoreHistogram[] =
    "SiteEngagementService.EngagementScore";

const char SiteEngagementMetrics::kEngagementScoreHistogramIsZero[] =
    "SiteEngagementService.EngagementScore.IsZero";

const char SiteEngagementMetrics::kOriginsWithMaxEngagementHistogram[] =
    "SiteEngagementService.OriginsWithMaxEngagement";

const char SiteEngagementMetrics::kOriginsWithMaxDailyEngagementHistogram[] =
    "SiteEngagementService.OriginsWithMaxDailyEngagement";

const char SiteEngagementMetrics::kPercentOriginsWithMaxEngagementHistogram[] =
    "SiteEngagementService.PercentOriginsWithMaxEngagement";

const char SiteEngagementMetrics::kEngagementTypeHistogram[] =
    "SiteEngagementService.EngagementType";

const char SiteEngagementMetrics::kEngagementBucketHistogramBase[] =
    "SiteEngagementService.EngagementScoreBucket_";

const char SiteEngagementMetrics::kDaysSinceLastShortcutLaunchHistogram[] =
    "SiteEngagementService.DaysSinceLastShortcutLaunch";

const char SiteEngagementMetrics::kScoreDecayedFromHistogram[] =
    "SiteEngagementService.ScoreDecayedFrom";

const char SiteEngagementMetrics::kScoreDecayedToHistogram[] =
    "SiteEngagementService.ScoreDecayedTo";

void SiteEngagementMetrics::RecordTotalSiteEngagement(double total_engagement) {
  UMA_HISTOGRAM_COUNTS_10000(kTotalEngagementHistogram, total_engagement);
}

void SiteEngagementMetrics::RecordTotalOriginsEngaged(int num_origins) {
  UMA_HISTOGRAM_COUNTS_10000(kTotalOriginsHistogram, num_origins);
}

void SiteEngagementMetrics::RecordMeanEngagement(double mean_engagement) {
  UMA_HISTOGRAM_COUNTS_100(kMeanEngagementHistogram, mean_engagement);
}

void SiteEngagementMetrics::RecordMedianEngagement(double median_engagement) {
  UMA_HISTOGRAM_COUNTS_100(kMedianEngagementHistogram, median_engagement);
}

void SiteEngagementMetrics::RecordEngagementScores(
    const std::vector<mojom::SiteEngagementDetails>& details) {
  if (details.size() == 0)
    return;

  std::map<int, int> score_buckets;
  for (size_t i = 0; i < base::size(kEngagementBucketHistogramBuckets); ++i)
    score_buckets[kEngagementBucketHistogramBuckets[i]] = 0;

  const double threshold_0 = std::numeric_limits<double>::epsilon();;
  for (const auto& detail : details) {
    double score = detail.total_score;
    UMA_HISTOGRAM_COUNTS_100(kEngagementScoreHistogram, score);
    UMA_HISTOGRAM_BOOLEAN(kEngagementScoreHistogramIsZero, score < threshold_0);

    auto bucket = score_buckets.lower_bound(score);
    if (bucket == score_buckets.end())
      continue;

    bucket->second++;
  }

  for (const auto& b : score_buckets) {
    std::string histogram_name =
        kEngagementBucketHistogramBase + base::NumberToString(b.first);

    base::LinearHistogram::FactoryGet(
        histogram_name, 1, 100, 10,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(b.second * 100 / details.size());
  }
}

void SiteEngagementMetrics::RecordOriginsWithMaxEngagement(int total_origins) {
  UMA_HISTOGRAM_COUNTS_100(kOriginsWithMaxEngagementHistogram, total_origins);
}

void SiteEngagementMetrics::RecordOriginsWithMaxDailyEngagement(
    int total_origins) {
  UMA_HISTOGRAM_COUNTS_100(kOriginsWithMaxDailyEngagementHistogram,
                           total_origins);
}

void SiteEngagementMetrics::RecordPercentOriginsWithMaxEngagement(
    double percentage) {
  UMA_HISTOGRAM_COUNTS_100(kPercentOriginsWithMaxEngagementHistogram,
                           percentage);
}

void SiteEngagementMetrics::RecordEngagement(
    SiteEngagementService::EngagementType type) {
  UMA_HISTOGRAM_ENUMERATION(kEngagementTypeHistogram, type,
                            SiteEngagementService::ENGAGEMENT_LAST);
}

void SiteEngagementMetrics::RecordDaysSinceLastShortcutLaunch(int days) {
  UMA_HISTOGRAM_COUNTS_100(kDaysSinceLastShortcutLaunchHistogram, days);
}

void SiteEngagementMetrics::RecordScoreDecayedFrom(double score) {
  UMA_HISTOGRAM_COUNTS_100(kScoreDecayedFromHistogram, score);
}

void SiteEngagementMetrics::RecordScoreDecayedTo(double score) {
  UMA_HISTOGRAM_COUNTS_100(kScoreDecayedToHistogram, score);
}

// static
std::vector<std::string>
SiteEngagementMetrics::GetEngagementBucketHistogramNames() {
  std::vector<std::string> histogram_names;
  for (size_t i = 0; i < base::size(kEngagementBucketHistogramBuckets); ++i) {
    histogram_names.push_back(
        kEngagementBucketHistogramBase +
        base::NumberToString(kEngagementBucketHistogramBuckets[i]));
  }

  return histogram_names;
}
