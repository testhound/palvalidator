#pragma once
#include "IBootstrapObserver.h"
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <filesystem>

namespace palvalidator::diagnostics
{
  /**
   * @brief CSV collector that writes bootstrap tournament diagnostics to two files.
   * 
   * DESIGN: Two-file normalized structure to avoid data redundancy
   * 
   * FILE 1: TournamentRuns.csv
   *   - One row per tournament
   *   - Contains tournament-level context (strategy, metric, sample size, etc.)
   *   - Indexed by RunID
   * 
   * FILE 2: Candidates.csv
   *   - Multiple rows per tournament (one per candidate method)
   *   - Contains candidate-specific data (method, rank, score, interval, etc.)
   *   - Links to TournamentRuns.csv via RunID
   * 
   * THREAD-SAFE: Uses mutex for concurrent writes
   */
  class CsvBootstrapCollector : public IBootstrapObserver {
  public:
    /**
     * @brief Construct collector with two CSV file paths.
     * 
     * @param tournamentRunsPath Path to TournamentRuns.csv (e.g., "output/tournament_runs.csv")
     * @param candidatesPath Path to Candidates.csv (e.g., "output/candidates.csv")
     * 
     * Both files will be created if they don't exist. If they exist and are non-empty,
     * headers will not be rewritten (append mode).
     */
    CsvBootstrapCollector(const std::string& tournamentRunsPath,
			  const std::string& candidatesPath);
    
    ~CsvBootstrapCollector();

    /**
     * @brief Process a bootstrap diagnostic record.
     * 
     * On first candidate of a tournament (new RunID):
     *   - Writes tournament context to TournamentRuns.csv
     * 
     * For every candidate:
     *   - Writes candidate details to Candidates.csv
     */
    void onBootstrapResult(const BootstrapDiagnosticRecord& record) override;

  private:
    void writeTournamentRunsHeaderIfNeeded();
    void writeCandidatesHeaderIfNeeded();
    
    void writeTournamentRun(const BootstrapDiagnosticRecord& record);
    void writeCandidate(const BootstrapDiagnosticRecord& record);

    std::string m_tournamentRunsPath;
    std::string m_candidatesPath;
    
    std::ofstream m_tournamentRunsFile;
    std::ofstream m_candidatesFile;
    
    std::mutex m_mutex;
    
    bool m_tournamentRunsHeaderWritten = false;
    bool m_candidatesHeaderWritten = false;
    
    // Track which RunIDs we've already written to TournamentRuns.csv
    std::unordered_set<uint64_t> m_writtenRunIDs;
  };


} // namespace palvalidator::diagnostics
