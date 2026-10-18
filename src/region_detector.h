#pragma once

void InstallFileHooks();

class RegionDetector {
public:
    int Update();
    void LogScores() const;

private:
    static constexpr int kMaxRegions = 32;
    double score_[kMaxRegions] = {};
    int current_ = -1;
};
