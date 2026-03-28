#include "car_rental/server.h"

#include <Poco/Util/HelpFormatter.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionCallback.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/ServerApplication.h>

#include <iostream>

namespace {

class ApiServerApp : public Poco::Util::ServerApplication
{
public:
    ApiServerApp() = default;

protected:
    void initialize(Poco::Util::Application& self) override
    {
        Poco::Util::ServerApplication::initialize(self);
    }

    void defineOptions(Poco::Util::OptionSet& options) override
    {
        Poco::Util::ServerApplication::defineOptions(options);

        options.addOption(Poco::Util::Option("help", "h", "Show help for the API server.")
                              .required(false)
                              .repeatable(false)
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handleHelp)));

        options.addOption(Poco::Util::Option("host", "", "Host interface to bind.")
                              .required(false)
                              .repeatable(false)
                              .argument("host")
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handleHost)));

        options.addOption(Poco::Util::Option("port", "p", "Port to listen on.")
                              .required(false)
                              .repeatable(false)
                              .argument("port")
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handlePort)));

        options.addOption(Poco::Util::Option("db-path", "", "Path to SQLite database file.")
                              .required(false)
                              .repeatable(false)
                              .argument("path")
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handleDatabasePath)));

        options.addOption(Poco::Util::Option("jwt-secret", "", "Secret used to sign JWT tokens.")
                              .required(false)
                              .repeatable(false)
                              .argument("secret")
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handleJwtSecret)));

        options.addOption(Poco::Util::Option("jwt-ttl", "", "JWT TTL in seconds.")
                              .required(false)
                              .repeatable(false)
                              .argument("seconds")
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handleJwtTtl)));

        options.addOption(Poco::Util::Option("manager-login", "", "Seed fleet manager login.")
                              .required(false)
                              .repeatable(false)
                              .argument("login")
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handleManagerLogin)));

        options.addOption(Poco::Util::Option("manager-password", "", "Seed fleet manager password.")
                              .required(false)
                              .repeatable(false)
                              .argument("password")
                              .callback(Poco::Util::OptionCallback<ApiServerApp>(this, &ApiServerApp::handleManagerPassword)));
    }

    int main(const std::vector<std::string>&) override
    {
        if (helpRequested_)
            return EXIT_OK;

        car_rental::ApiServer server(config_);
        server.start();
        std::cout << "Car rental API is listening on " << config_.host << ":" << server.port() << std::endl;
        waitForTerminationRequest();
        server.stop();
        return EXIT_OK;
    }

private:
    void handleHelp(const std::string&, const std::string&)
    {
        helpRequested_ = true;
        stopOptionsProcessing();
        Poco::Util::HelpFormatter formatter(options());
        formatter.setCommand(commandName());
        formatter.setUsage("OPTIONS");
        formatter.setHeader("REST API for the car rental management homework.");
        formatter.format(std::cout);
    }

    void handleHost(const std::string&, const std::string& value)
    {
        config_.host = value;
    }

    void handlePort(const std::string&, const std::string& value)
    {
        config_.port = std::stoi(value);
    }

    void handleDatabasePath(const std::string&, const std::string& value)
    {
        config_.databasePath = value;
    }

    void handleJwtSecret(const std::string&, const std::string& value)
    {
        config_.jwtSecret = value;
    }

    void handleJwtTtl(const std::string&, const std::string& value)
    {
        config_.jwtTtlSeconds = std::stol(value);
    }

    void handleManagerLogin(const std::string&, const std::string& value)
    {
        config_.managerLogin = value;
    }

    void handleManagerPassword(const std::string&, const std::string& value)
    {
        config_.managerPassword = value;
    }

    bool helpRequested_{false};
    car_rental::ServerConfig config_{};
};

} // namespace

int main(int argc, char** argv)
{
    ApiServerApp app;
    return app.run(argc, argv);
}
