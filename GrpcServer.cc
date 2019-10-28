#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <iostream>
#include <memory>
#include <string>
#include <unistd.h>

#include <grpc++/grpc++.h>

#ifdef BAZEL_BUILD
#include "Microcontroller/grpcMessageSerialization.grpc.pb.h"
#else
#include "grpcMessageSerialization.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpcserver::messagePassing;
using grpcserver::setVariables;
using grpcserver::variablesSet;

static int values[5];
static int i;

static int callback(void *data, int argc, char **argv, char **azColName)
{

    values[i] = atoi(argv[0]);
    i++;

    return 0;
}

class GrpcServer
{
public:
    GrpcServer(std::shared_ptr<Channel> channel)
        : stub_(messagePassing::NewStub(channel)) {}

    // Assembles the client's payload, sends it and presents the response back
    // from the server.
    bool sendVariables(int value[5])
    {
        //  Variables in value[5] are in order:
        //      (names in the database)
        //      acceleratingVoltage;
        //      deflectingPolarity;
        //      deflectingVoltage;
        //      magneticArc;
        //      magnetizingCurrent;

        // Data we are sending to the server.
        setVariables request;
        request.set_accelv(value[0]);           //Range 0 to 250 volts
        request.set_deflectingpolarity(value[1]); // 0 = off; 1 = positive; 2 = negative
        request.set_deflectingv(value[2]);      //Range 50 to 250 volts
        request.set_magneticarc(value[3]);      // 0 = off; 1 = clockwise; 2 = counterclockwise
        request.set_current(value[4]);          //Range 0.0 to 3.0 amps
                                                //Represented as an int as 0 to 300

        // Container for the data we expect from the server.
        variablesSet reply;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // The actual RPC.
        Status status = stub_->sendVariables(&context, request, &reply);

        // Act upon its status.
        if (status.ok())
        {
            return reply.flag();
        }
        else
        {
            std::cout << status.error_code() << ": " << status.error_message()
                      << std::endl;
            return "RPC failed";
        }
    }

private:
    std::unique_ptr<messagePassing::Stub> stub_;
};

int main(int argc, char **argv)
{
    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint (in this case,
    // localhost at port 50051). We indicate that the channel isn't authenticated
    // (use of InsecureChannelCredentials()).

    //setup database
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    // Open database
    rc = sqlite3_open("RemotePhysLab.db", &db);

    if (rc)
    {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    }
    else
    {
        // fprintf(stdout, "Opened database successfully\n");
    }

    // Create SQL statement
    const char *sql = "DROP TABLE IF EXISTS DATA;"
                      "CREATE TABLE DATA("
                      "Variables	CHAR(50)	PRIMARY KEY	NOT NULL,"
                      "Values_	INT	NOT NULL);"
                      "DROP TABLE IF EXISTS QUEUE;"
                      "CREATE TABLE QUEUE("
                      "QueuePosition	INTEGER	PRIMARY KEY AUTOINCREMENT,"
                      "ID 	INT 	UNIQUE	NOT NULL);";

    // Execute table creation
    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    sql = "INSERT INTO DATA (Variables,Values_) "
          "VALUES ('acceleratingVoltage', 0); "
          "INSERT INTO DATA (Variables,Values_) "
          "VALUES ('deflectingVoltage', 50); "
          "INSERT INTO DATA (Variables,Values_) "
          "VALUES ('magnetizingCurrent', 0); "
          "INSERT INTO DATA (Variables,Values_) "
          "VALUES ('deflectingPolarity', 0); "
          "INSERT INTO DATA (Variables,Values_) "
          "VALUES ('magneticArc', 0); ";

    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }
    else
    {
        //fprintf(stdout, "Table created successfully\n");
    }

    GrpcServer messageSession(grpc::CreateChannel(
        "69.88.163.30:50051", grpc::InsecureChannelCredentials()));

    while (true)
    {
        //  std::cout<<"sleeping" <<std::endl;
        usleep(3000000);

        //Reading Database
        sqlite3 *db;
        char *zErrMsg = 0;
        int rc;
        i = 0;
        // Open database
        rc = sqlite3_open("RemotePhysLab.db", &db);
        const char *sql = "SELECT Values_ from DATA order by Variables;";

        rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

        if (rc != SQLITE_OK)
        {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        else
        {
            // fprintf(stdout, "Table read successfully\n");
        }
        sqlite3_close(db);
        //int hi[7] = {1,2,3,4,5,6,7};
        //std::cout<<values[1]<<std::endl;

        bool reply = messageSession.sendVariables(values);
        //std::cout << "Greeter received: " << reply << std::endl;
    }

    return 0;
}
