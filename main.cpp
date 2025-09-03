#include "crow.h"

int main()
{
    crow::SimpleApp app;

    // Set the base folder for templates
    crow::mustache::set_base("templates");

    CROW_ROUTE(app, "/")([](){
        crow::mustache::context ctx;
        auto rendered = crow::mustache::load("index.html").render(ctx);
        return rendered;
    });

    app.port(18080).run();
}
