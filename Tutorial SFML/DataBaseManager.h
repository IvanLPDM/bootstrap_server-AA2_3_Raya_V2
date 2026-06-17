#pragma once
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <string>
#include <vector>
#include <cstdint>

struct RankingEntry
{
    short rank;
    std::string nickname;
    short points;
    short wins;
    short losses;
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
    void UpdateRanking(const std::vector<std::pair<std::string, short>>& results, short numPlayers);
    std::vector<RankingEntry> GetTopTenPlayers();
    bool GetPlayerRanking(const std::string& nickname, short& outRank, short& outPoints, short& outWins, short& outLosses);

private:
    sql::Driver* _driver;
    sql::Connection* _con;
};

