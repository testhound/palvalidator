#ifndef RUNPARAMETERS_H
#define RUNPARAMETERS_H

#include <string>

namespace mkc_timeseries
{
    class RunParameters
    {
        public:
            RunParameters(){};

            std::string getConfigFile1Path() { return mConfigFile1Path; }
            std::string getSearchConfigFilePath() { return mSearchConfigFilePath; }
            std::string getApiConfigFilePath() { return mApiConfigFilePath; }
            std::string getHourlyDataFilePath() { return mHourlyDataFilePath; }
            std::string getEodDataFilePath() { return mEodDataFilePath; }
            std::string getApiSource() { return mApiSource; }
            bool shouldUseApi() { return mUseApi; }

            void setUseApi(bool useApi) { mUseApi = useApi; }
            void setConfig1FilePath(std::string filename) { mConfigFile1Path = filename; }
            void setSearchConfigFilePath(std::string filename) { mSearchConfigFilePath = filename; }
            void setApiConfigFilePath(std::string filename) { mApiConfigFilePath = filename; }
            void setHourlyDataFilePath(std::string filename) { mHourlyDataFilePath = filename; }
            void setEodDataFilePath(std::string filename) { mEodDataFilePath = filename; }
            void setApiSource(std::string source) { mApiSource = source; }

        private:
            bool mUseApi;

            std::string mConfigFile1Path;
            std::string mSearchConfigFilePath;

            std::string mApiConfigFilePath;
            std::string mApiSource;
            std::string mHourlyDataFilePath;
            std::string mEodDataFilePath;

    };
}

#endif