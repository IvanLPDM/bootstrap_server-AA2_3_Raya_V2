#pragma once
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <string>
#include <vector>

struct RankingEntry
{
    int rank;
    std::string nickname;
    int points;
    int wins;
    int losses;
};

class DataBaseManager
{
public:
    DataBaseManager();
    ~DataBaseManager();

    void ConnectDatabase();

    bool RegisterUser(const std::string& nickname, const std::string& password);
    bool LoginUser(const std::string& nickname, const std::string& password);

    // ranking
    void UpdateRanking(const std::vector<std::pair<std::string, int>>& results, int numPlayers);
    std::vector<RankingEntry> GetTopTenPlayers();
    bool GetPlayerRanking(const std::string& nickname, int& outRank, int& outPoints, int& outWins, int& outLosses);

private:
    sql::Driver* _driver;
    sql::Connection* _con;
};

